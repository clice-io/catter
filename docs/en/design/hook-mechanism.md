# Hook Mechanism (inject runtime implementation)

> This page describes the internals of the **inject runtime**. Handler authors usually do not need to read it; it answers the platform-level question "how are commands captured". For the capabilities and limitations handlers care about, see [Runtime](runtime.md).

The hook is the capture carrier of the inject runtime: a shared library loaded into every process spawned by the build system. Its sole job is to intercept process creation calls and rewrite them so that every child process goes through `catter-proxy` instead of executing directly.

The hook implementation is entirely platform-specific. Unix and Windows use fundamentally different interception techniques.

## Unix (Linux and macOS) -- LD_PRELOAD Interception

The Unix hook is a shared library loaded via the dynamic linker's preload mechanism:

- **Linux**: `libcatter-hook-unix.so` loaded via `LD_PRELOAD`
- **macOS**: `libcatter-hook-unix.dylib` loaded via `DYLD_INSERT_LIBRARIES`

### Intercepted Functions

The hook replaces all standard POSIX functions for creating new processes:

**exec family**:

- `execve()`, `execv()`
- `execvpe()`, `execvp()`, `execvP()`
- `execl()`, `execlp()`, `execle()`

**posix_spawn family**:

- `posix_spawn()`
- `posix_spawnp()`

On Linux, the hook uses `dlsym(RTLD_NEXT, "execve")` to obtain the original function pointer from the next library in the load order. It then provides a replacement function with the same signature. When the replacement is called, the hook can inspect and modify the arguments before optionally calling the original.

On macOS, the hook uses the `DYLD_INTERPOSE` macro to replace functions at the dyld level, which is the preferred technique on that platform.

### Key Classes

The hook is implemented as a set of cooperating classes:

- **`Session`** -- Reads and stores session information from the environment (the proxy path, the self command ID and the session's pipe).
- **`Resolver`** -- Resolves the target executable path. Handles PATH lookups, relative path resolution, and edge cases like missing executables.
- **`Command` / `build_proxy_command()`** -- Builds the rewritten proxy command, substituting `catter-proxy` for the original executable.
- **`SanitizedEnv` / `sanitize_environment()`** -- Scrubs the environment before the real `execve` is called. Removes catter variables and strips the hook library from the preload variable.
- **`Executor`** -- Orchestrates the interception. Validates the session, resolves the executable, builds the proxy command, cleans the environment, and calls the original function; it also holds the real `execve` / `posix_spawn` function pointers (the earlier standalone `Linker` class has been folded into it).

### Hook Initialization

The hook library is loaded via the dynamic linker's constructor mechanism (`on_load`, `__attribute__((constructor))`). During initialization, the library:

1. Sets up logging (in debug builds, to `log/catter-hook.log` in the catter data directory)
2. Calls `Executor::init()`: reads session state from the environment and resolves the real `execve` / `posix_spawn` function pointers (`dlsym(RTLD_NEXT, ...)` on Linux, direct system symbols on macOS)

### Environment Variables

The hook reads session information from these environment variables, set by the proxy when it launches the build command:

| Variable                     | Purpose                                    |
| ---------------------------- | ------------------------------------------ |
| `__key_catter_proxy_path_v1` | Absolute path to the `catter-proxy` binary |
| `__key_catter_command_id_v1` | Session ID of the parent process           |
| `__key_catter_ipc_v1`        | The pipe of the catter session             |

### Interception Flow

When any process creation function is called (e.g., `execve("/usr/bin/g++", argv, envp)`):

1. The hook's replacement function is entered.

2. **`Executor`** validates the session. If the session is invalid (missing environment variables), it builds an error command that reports the problem to the daemon.

3. **`Resolver`** resolves the target executable to an absolute path. For functions like `execvp()` and `execvpe()`, it searches directories in `PATH`. For `execve()`, it resolves relative to the current directory.

4. **`Command`** (`build_proxy_command()`) constructs the proxy command:

   ```
   <proxy_path> -p <self_id> --exec <resolved_path> --ipc <pipe> -- <original_argv...>
   ```

   The original `argv[0]` and all subsequent arguments are preserved after the `--` separator.

5. **`SanitizedEnv`** (`sanitize_environment()`) modifies the environment array:
   - Removes `__key_catter_proxy_path_v1`, `__key_catter_command_id_v1` and `__key_catter_ipc_v1`
   - Strips the hook library name from `LD_PRELOAD` (or `DYLD_INSERT_LIBRARIES`)
   - If `LD_PRELOAD` becomes empty after stripping, removes it entirely

6. **`Executor`** calls the **real** `execve()` with the rewritten command (through the function pointers resolved during initialization).

7. If `execve` succeeds, it does not return (the current process image is replaced). If it fails, the hook restores `errno` and returns the error to the caller.

### Why Clean the Environment?

This step is critical. If the hook library remained in `LD_PRELOAD` when `catter-proxy` is launched:

1. `catter-proxy` itself would be hooked
2. When the proxy tries to execute the actual compiler, the hook would intercept that call
3. The hook would rewrite it to launch another `catter-proxy`
4. This creates **infinite recursion**

By stripping the hook from `LD_PRELOAD`, the proxy executes without interception. The proxy re-adds `LD_PRELOAD` only when it launches a command with the `INJECT` action, ensuring the hook is attached to the right processes.

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

### Hook DLLs

A process loads only DLLs of its own architecture, so there is one hook DLL per architecture, each installed next to `catter-proxy.exe`:

| DLL                     | Loaded into                                                      |
| ----------------------- | ---------------------------------------------------------------- |
| `catter-hook-x64.dll`   | x64 processes, x64 processes emulated on ARM64, ARM64EC programs |
| `catter-hook-arm64.dll` | native ARM64 processes                                           |

Every Windows build of catter carries both: on Windows on ARM, a build mixes native ARM64 tools with emulated x64 ones. A 32-bit (x86) process has no hook DLL yet: it runs without one, as it would without catter, and the commands it starts are not captured. catter-proxy logs a warning naming the program.

Each DLL is built without AddressSanitizer and with the C++ runtime linked in, importing only Windows' own DLLs, UCRT among them: it runs inside every process of the build, which may lack other runtime DLLs, and Detours' search of the address space reads ASan's shadow memory.

### Environment Variables

| Variable            | Purpose                                        |
| ------------------- | ---------------------------------------------- |
| `CATTER_IPC_ID`     | Session ID of the parent process               |
| `CATTER_IPC_PIPE`   | The pipe of the catter session                 |
| `CATTER_PROXY_PATH` | Absolute path to the `catter-proxy.exe` binary |

### DLL Injection Process

When `catter-proxy` (in injector mode) needs to start a build command with the hook attached:

1. **Create the target process suspended**. The proxy calls `CreateProcessW()` with the `CREATE_SUSPENDED` flag. The process is created but its main thread does not run.

2. **Set environment variables**. `CATTER_IPC_ID`, `CATTER_IPC_PIPE` and `CATTER_PROXY_PATH` are set in the target process environment before creation (passed via the environment block).

3. **Find the process's architecture**. `GetProcessInformation(ProcessMachineTypeInfo)` gives the machine whose code the process runs: x64 for x64 processes emulated on ARM64 and for ARM64EC ones, which `IsWow64Process2` cannot tell from native ARM64 processes. On Windows 10, which has no `ProcessMachineTypeInfo` and emulates no x64, `IsWow64Process2` gives it.

4. **Add the hook DLL to the process's imports**. `DetourUpdateProcessWithDll()` adds the DLL of that architecture to the import table of the suspended executable, so the loader maps it before any of the program's own code runs. The loader reads the DLL's path there in the process's own ANSI code page, which a program can make UTF-8, so the path must be ASCII: a path that is not goes by its 8.3 short name, or, on a volume that keeps no short names, by a copy of the DLL in catter's data directory.

5. **Resume the main thread**. The proxy calls `ResumeThread()` to let the target process start executing. The loader maps the hook DLL with the program's other imports, and its hooks are active before the program's entry point runs.

### Hooked Windows APIs

The hook DLL attaches Detours hooks to these functions in one transaction during `DLL_PROCESS_ATTACH`:

- **`CreateProcessA()`** -- ANSI version of process creation
- **`CreateProcessW()`** -- Wide-character version of process creation

A process a program starts under another account's token (`CreateProcessAsUser`, `CreateProcessWithTokenW`, `CreateProcessWithLogonW`) runs without the hook and is not captured: `catter-proxy` would have to run under that token too, and that account may be unable to start it or to reach the session's pipe.

Detours works by:

1. Saving the first few instructions of the target function
2. Overwriting them with a jump to the detour function
3. Providing a trampoline that contains the saved instructions followed by a jump back, so the original function can still be called

### Hook Detour Logic

When `CreateProcessA` or `CreateProcessW` is called by the hooked process:

1. **Extract the command line**. The hook reads `lpApplicationName` and `lpCommandLine`.

2. **Resolve the absolute path**. The hook resolves the executable using `resolve_abspath()`, which searches the program directory, current directory, System32, Windows directory, and `PATH` (matching the standard Windows search order).

3. **Rewrite the command line**:

   ```
   {proxy_path} -p {ipc_id} --exec {resolved_path} --ipc {pipe} -- {original_cmdline}
   ```

   The `lpApplicationName` is set to `nullptr` so that the command line is parsed by `CreateProcess` normally.

4. **Call the original `CreateProcess`** via the Detours trampoline with the modified command line.

### Key Difference from Unix

On Unix, `LD_PRELOAD` is an environment variable inherited automatically by all child processes. The hook is "viral" by default -- every subprocess gets it.

On Windows, there is no such mechanism. The hook DLL must be explicitly injected into each new process. This happens because:

1. The hook's `CreateProcess` detour intercepts every process creation
2. It rewrites the command to go through `catter-proxy`
3. When the proxy decides to execute with `INJECT` action, it performs DLL injection into the new process (create suspended, inject, resume)

This means the proxy is responsible for re-injecting the hook into each generation of child processes, maintaining the recursive interception chain. Processes of different architectures can alternate along the chain: each gets the hook DLL of its own.

### DLL Lifecycle

```
DLL_PROCESS_ATTACH:
  DetourRestoreAfterWith()   -- Restore the import table catter-proxy changed
  DetourTransactionBegin()
  DetourAttach(...)          -- Hook CreateProcessA and CreateProcessW
  DetourTransactionCommit()  -- Activate all hooks at once
```

`DisableThreadLibraryCalls()` is called during attach to suppress `DLL_THREAD_ATTACH` and `DLL_THREAD_DETACH` notifications, reducing overhead. The DLL is an import of the executable, unloaded only with the process, so `DLL_PROCESS_DETACH` has nothing to undo. It exports ordinal 1, as every DLL Detours injects must.

## Platform Comparison

| Aspect               | Linux                                                      | macOS                           | Windows                                |
| -------------------- | ---------------------------------------------------------- | ------------------------------- | -------------------------------------- |
| Hook library         | `libcatter-hook-unix.so`                                   | `libcatter-hook-unix.dylib`     | `catter-hook-{x64,arm64}.dll`          |
| Injection method     | `LD_PRELOAD` env var                                       | `DYLD_INSERT_LIBRARIES` env var | `DetourUpdateProcessWithDll`           |
| Interposition        | `dlsym(RTLD_NEXT, ...)`                                    | `DYLD_INTERPOSE` macro          | Detours trampoline hooking             |
| Intercepted APIs     | `execve`, `execvp`, `posix_spawn`, etc.                    | Same as Linux                   | `CreateProcessA`, `CreateProcessW`     |
| Recursive hooking    | Automatic (env inherited)                                  | Automatic (env inherited)       | Explicit (DLL re-injected per process) |
| Env vars for session | `__key_catter_proxy_path_v1`, `__key_catter_command_id_v1` | Same as Linux                   | `CATTER_PROXY_PATH`, `CATTER_IPC_ID`   |
