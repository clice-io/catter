# Hook Mechanism (hook backend implementation)

> This page describes the internals of the **hook backend**. Handler authors usually do not need to read it; it answers the platform-level question "how are commands captured". For the capabilities and limitations handlers care about, see [Capture backends](capture-backends.md).

The hook is what the hook backend captures a build with: a shared library loaded into every process spawned by the build system. It intercepts process creation calls. On Linux and macOS it rewrites them so that every child process goes through `catter-proxy` instead of executing directly; on Windows it asks catter itself what to do with each one and starts the program as catter decides.

The hook implementation is entirely platform-specific. Unix and Windows use fundamentally different interception techniques.

## Unix (Linux and macOS) -- LD_PRELOAD Interception

The Unix hook is a shared library loaded via the dynamic linker's preload mechanism:

- **Linux**: `libcatter-hook-unix.so` loaded via `LD_PRELOAD`
- **macOS**: `libcatter-hook-unix.dylib` loaded via `DYLD_INSERT_LIBRARIES`

### Intercepted Functions

The hook replaces all standard POSIX functions for creating new processes:

**exec family**:

- `execve()`, `execv()`
- `execvp()`, `execvpe()` (Linux), `execvP()` (macOS)
- `execl()`, `execlp()`, `execle()`

**posix_spawn family**:

- `posix_spawn()`
- `posix_spawnp()`

On Linux, the hook uses `dlsym(RTLD_NEXT, "execve")` to obtain the original function pointer from the next library in the load order. It then provides a replacement function with the same signature. When the replacement is called, the hook can inspect and modify the arguments before optionally calling the original.

On macOS, the hook uses the `DYLD_INTERPOSE` macro to replace functions at the dyld level, which is the preferred technique on that platform.

dyld ends a program that cannot load a library inserted into it, and the hook is built for catter's own architecture, which Apple's own tools (arm64e) and Intel programs under Rosetta (x86_64) do not run as. Before it inserts the hook, `catter-proxy` asks dyld which architecture the program runs as (`macho_best_slice`; a script runs as its interpreter): a program of another runs without the hook, and what it starts is not captured. Where System Integrity Protection is on, dyld drops the hook from Apple's tools all the same, so a build is captured down to the Apple tools it runs and no further: `/usr/bin/make`, or `/bin/sh`, which Ninja runs every command with.

### Key Classes

The hook is implemented as a set of cooperating classes:

- **`find_program()`** (kotatsu) -- Finds the target executable as the exec function would, in `PATH` or the current directory; a program that is missing or cannot run fails as the call would.
- **`proxy_environment()`** -- The environment `catter-proxy` gets: the one the program asked for, without the hook among the preloaded libraries, and with the variables that name the session.
- **`Executor`** -- Orchestrates the interception. It takes the session from the environment the library loaded with, finds the executable, and calls the original function with `catter-proxy` in the program's place; it also holds the real `execve` / `posix_spawn` function pointers.

### Hook Initialization

The hook library is loaded via the dynamic linker's constructor mechanism (`on_load`, `__attribute__((constructor))`). During initialization, the library:

1. Sets up logging (in debug builds, to `log/catter-hook.log` in the catter data directory)
2. Calls `Executor::init()` with the environment and the real `execve` / `posix_spawn` function pointers (`dlsym(RTLD_NEXT, ...)` on Linux, direct system symbols on macOS)

### Environment Variables

The hook reads session information from these environment variables, which the proxy sets when it runs its command with the hook (`RUN`), the command ID being that command's own:

| Variable                     | Purpose                                    |
| ---------------------------- | ------------------------------------------ |
| `__key_catter_proxy_path_v1` | Absolute path to the `catter-proxy` binary |
| `__key_catter_command_id_v1` | The ID of the command the process runs     |
| `__key_catter_ipc_v1`        | The pipe of the catter session             |

### Interception Flow

When any process creation function is called (e.g., `execve("/usr/bin/g++", argv, envp)`):

1. The hook's replacement function is entered.

2. When the environment the library loaded with names no `catter-proxy`, the call goes to the system as it is: the process has no session.

3. **`Executor`** finds the target executable with kotatsu's `find_program()`. For functions like `execvp()` and `execvpe()`, it searches the directories of `PATH`. For `execve()`, a path without a `/` is taken from the current directory.

4. **`Executor`** constructs the proxy command:

   ```
   <proxy_path> <resolved_path> <original_argv...>
   ```

   The original `argv[0]` and all subsequent arguments follow the resolved path.

5. **`proxy_environment()`** modifies the environment array:
   - Strips the hook library name from `LD_PRELOAD` (or `DYLD_INSERT_LIBRARIES`)
   - If `LD_PRELOAD` becomes empty after stripping, removes it entirely
   - Puts `__key_catter_ipc_v1` and `__key_catter_command_id_v1` in place of any the program's environment has, as the process loaded with them: a program may start another with an environment of its own making, as `env -i` does, and `catter-proxy` still finds its session there

6. **`Executor`** calls the **real** `execve()` with the rewritten command (through the function pointers resolved during initialization).

7. If `execve` succeeds, it does not return (the current process image is replaced). If it fails, the hook restores `errno` and returns the error to the caller.

### Why Clean the Environment?

This step is critical. If the hook library remained in `LD_PRELOAD` when `catter-proxy` is launched:

1. `catter-proxy` itself would be hooked
2. When the proxy tries to execute the actual compiler, the hook would intercept that call
3. The hook would rewrite it to launch another `catter-proxy`
4. This creates **infinite recursion**

By stripping the hook from `LD_PRELOAD`, the proxy executes without interception. The proxy re-adds `LD_PRELOAD` only when it launches a command with the `RUN` action, ensuring the hook is attached to the right processes.

### Recursive Hooking

Since `LD_PRELOAD` is inherited by child processes through the environment, interception is automatically recursive. Consider this build scenario:

```
make
  -> sh -c "gcc main.c -o main"
     -> gcc main.c -o main
        -> cc1 main.c -o main.s
        -> as main.s -o main.o
        -> ld main.o -o main
```

Every process in this tree inherits `LD_PRELOAD` and gets hooked. Each invocation goes through `catter-proxy`, which asks the daemon what to do. The build system is completely unaware of the interception.

---

## Windows -- DLL Injection and API Hooking

The Windows hook uses a fundamentally different approach because Windows has no equivalent to `LD_PRELOAD`. Instead, catter combines:

1. **DLL injection** -- to load the hook library into target processes
2. **API hooking via [Microsoft Detours](https://github.com/microsoft/Detours)** -- to intercept `CreateProcess` calls within those processes

The Windows hook runs no `catter-proxy`. It runs inside the caller's own `CreateProcess` call, where it can do what `catter-proxy` does on Unix: it speaks to catter over the session's pipe, and it starts the program itself. Windows gives catter the exit code of any process it holds a handle to, so catter needs no process of its own beside each program to learn how it ended.

### Hook DLLs

A process loads only DLLs of its own architecture, so there is one hook DLL per architecture, each installed next to `catter.exe`:

| DLL                     | Loaded into                                                      |
| ----------------------- | ---------------------------------------------------------------- |
| `catter-hook-x64.dll`   | x64 processes, x64 processes emulated on ARM64, ARM64EC programs |
| `catter-hook-arm64.dll` | native ARM64 processes                                           |

Every Windows build of catter carries both: on Windows on ARM, a build mixes native ARM64 tools with emulated x64 ones. A 32-bit (x86) process has no hook DLL yet: it runs without one, as it would without catter, and the commands it starts are not captured. catter logs a warning naming the program. A handler that drops a command of such a program has catter end it at once, before its caller resumes it.

Each DLL is built without AddressSanitizer and with the C++ runtime linked in, importing only Windows' own DLLs, UCRT among them: it runs inside every process of the build, which may lack other runtime DLLs, and Detours' search of the address space reads ASan's shadow memory.

### The Session of a Process

The hook in a process learns the session it reports to from catter, not from the environment: catter copies a Detours payload into each process it injects the hook into, holding the session's pipe and the ID of the command the process runs, or, for a command that is not to run, the exit code the process ends with as soon as the hook loads. The build's environment stays as the build made it, and each program gets its own command ID though its caller passes it the environment it has.

### DLL Injection Process

catter injects the hook into the build command, which it starts itself, and into every program the hook starts:

1. **The process is suspended**. catter starts the build command with kotatsu's `kota::sys::spawn`, whose `windows_before_start` callback receives the process created, in a job object of its own, before its main thread runs. A program the hook starts is created suspended by the hook, which sends catter its process ID; catter opens the process.

2. **Find the process's architecture**. `GetProcessInformation(ProcessMachineTypeInfo)` gives the machine whose code the process runs: x64 for x64 processes emulated on ARM64 and for ARM64EC ones, which `IsWow64Process2` cannot tell from native ARM64 processes. On Windows 10, which has no `ProcessMachineTypeInfo` and emulates no x64, `IsWow64Process2` gives it.

3. **Add the hook DLL to the process's imports**. `DetourUpdateProcessWithDll()` adds the DLL of that architecture to the import table of the suspended executable, so the loader maps it before any of the program's own code runs. The loader reads the DLL's path there in the process's own ANSI code page, which a program can make UTF-8, so the path must be ASCII: a path that is not goes by its 8.3 short name, or, on a volume that keeps no short names, by a copy of the DLL in catter's data directory.

4. **Give the hook its session**. `DetourCopyPayloadToProcess()` copies the session's pipe and the command's ID into the process.

5. **Resume the main thread**. `spawn` resumes the build command once the callback returns; the hook resumes a program once catter has answered. The loader maps the hook DLL with the program's other imports, and its hooks are active before the program's entry point runs.

### Hooked Windows APIs

The hook DLL attaches Detours hooks to these functions in one transaction during `DLL_PROCESS_ATTACH`:

- **`CreateProcessA()`** -- ANSI version of process creation
- **`CreateProcessW()`** -- Wide-character version of process creation

A process a program starts under another account's token (`CreateProcessAsUser`, `CreateProcessWithTokenW`, `CreateProcessWithLogonW`) runs without the hook and is not captured: that account may be unable to reach the session's pipe.

Detours works by:

1. Saving the first few instructions of the target function
2. Overwriting them with a jump to the detour function
3. Providing a trampoline that contains the saved instructions followed by a jump back, so the original function can still be called

### Hook Detour Logic

When `CreateProcessA` or `CreateProcessW` is called by the hooked process:

1. **Resolve the program**. The hook resolves it with `resolve_program()`: `lpApplicationName` made absolute from the current directory, or else the first token of the command line, looked for as `CreateProcess` looks for it (kotatsu's `find_create_process_program`: the program's directory, the current directory, the system directories, the Windows directory, then `PATH`); a name without quotes is read up to each blank in turn, as `CreateProcess` reads `C:\Program Files\tool.exe a b`. When there is no such file, the call goes to the original `CreateProcess` unchanged, which fails as it does without catter.

2. **Connect to the session**. The hook opens the session's pipe. When it cannot, as for a build server an earlier session started, whose session is gone, the call goes to the original `CreateProcess` unchanged, and the program runs uncaptured.

3. **Ask catter**. The hook reads the command as `CreateProcess` would start it: the arguments of the command line, the environment block (or the caller's own environment), and the working directory. It sends `MAKE_DECISION`, and waits for catter's answer, as `CreateProcess` itself waits for nothing else.

4. **Start the program suspended**. The hook calls the original `CreateProcess` with the caller's own arguments, flags, handles and `STARTUPINFO`, adding only `CREATE_SUSPENDED`. What a handler changed of the command is written anew: its program, looked for as `CreateProcess` would look for it from the command's working directory; its command line, written from its arguments, only when it changed them; its environment; its working directory. The rest goes on as the caller wrote it, so `cmd.exe`, and any other program that reads its command line itself, reads what it would without catter. When the program cannot start, the hook reports it (`REPORT_ERROR`) and fails the call with the system's error.

5. **Let catter take it**. The hook sends the process ID (`started`). catter opens the process and makes it run as decided: it injects the hook, or, for a command that is not to run, injects it set to end the process with exit code 0 (dropped) or 1 (the session failed) as soon as it loads, before the program's own code runs. catter then waits for the process to exit, and tells the handlers its exit code. When catter cannot make the program run as decided, it reports the error, and the hook ends the program with exit code 1.

6. **Resume it**. The hook resumes the main thread, unless the caller asked for the process suspended, and returns. The caller gets the program's own process and thread handles and IDs.

The caller holds the program itself, as it would without catter. What a caller does to the process it starts works as it does without catter: MSBuild's tracker, which starts each tool suspended and injects its file tracking into it before it runs, a debugger, or a build tool that keeps the process IDs of its jobs.

### Key Difference from Unix

On Unix, `LD_PRELOAD` is an environment variable inherited automatically by all child processes. The hook is "viral" by default -- every subprocess gets it.

On Windows, there is no such mechanism. The hook DLL must be explicitly injected into each new process: catter injects it into each program the hook starts, while the program is suspended. Processes of different architectures can alternate along the chain: each gets the hook DLL of its own.

### DLL Lifecycle

```
DLL_PROCESS_ATTACH:
  DetourRestoreAfterWith()   -- Restore the import table catter changed
  DetourFindPayloadEx()      -- Read the session catter copied into the process
  TerminateProcess(...)      -- End a process whose command is not to run
  DetourTransactionBegin()
  DetourAttach(...)          -- Hook CreateProcessA and CreateProcessW
  DetourTransactionCommit()  -- Activate all hooks at once
```

`DisableThreadLibraryCalls()` is called during attach to suppress `DLL_THREAD_ATTACH` and `DLL_THREAD_DETACH` notifications, reducing overhead. The DLL is an import of the executable, unloaded only with the process, so `DLL_PROCESS_DETACH` has nothing to undo. It exports ordinal 1, as every DLL Detours injects must.

## Platform Comparison

| Aspect               | Linux                                                                             | macOS                           | Windows                                |
| -------------------- | --------------------------------------------------------------------------------- | ------------------------------- | -------------------------------------- |
| Hook library         | `libcatter-hook-unix.so`                                                          | `libcatter-hook-unix.dylib`     | `catter-hook-{x64,arm64}.dll`          |
| Injection method     | `LD_PRELOAD` env var                                                              | `DYLD_INSERT_LIBRARIES` env var | `DetourUpdateProcessWithDll`           |
| Interposition        | `dlsym(RTLD_NEXT, ...)`                                                           | `DYLD_INTERPOSE` macro          | Detours trampoline hooking             |
| Intercepted APIs     | `execve`, `execvp`, `posix_spawn`, etc.                                           | Same as Linux                   | `CreateProcessA`, `CreateProcessW`     |
| Recursive hooking    | Automatic (env inherited)                                                         | Automatic (env inherited)       | Explicit (DLL re-injected per process) |
| Session              | `__key_catter_proxy_path_v1`, `__key_catter_command_id_v1`, `__key_catter_ipc_v1` | Same as Linux                   | A Detours payload in each process      |
| Who starts a command | `catter-proxy`                                                                    | Same as Linux                   | The hook, in the caller's own call     |
