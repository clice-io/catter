# Hook Mechanism

The hook is the lowest-level component of catter. It is a shared library that gets loaded into every process spawned by the build system. Its sole job is to intercept process creation calls and rewrite them so that every child process goes through `catter-proxy` instead of executing directly.

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

- **`Session`** -- Reads and stores session information from the environment. Provides the proxy path and parent session ID.
- **`Resolver`** -- Resolves the target executable path. Handles PATH lookups, relative path resolution, and edge cases like missing executables.
- **`CmdBuilder`** -- Constructs the rewritten command that invokes `catter-proxy` instead of the original executable.
- **`EnvGuard`** -- RAII guard that scrubs the environment before the real `execve` is called. Removes catter variables and strips the hook library from `LD_PRELOAD`.
- **`Executor`** -- Orchestrates the interception. Validates the session, resolves the executable, builds the proxy command, cleans the environment, and calls the original function.
- **`Linker`** -- Abstraction for the real system call. Wraps `dlsym(RTLD_NEXT, ...)` to call the original `execve` or `posix_spawn`.

### Hook Initialization

The hook library is loaded via the dynamic linker's constructor mechanism (`__attribute__((constructor))` or equivalent). During initialization, the library:

1. Sets up logging (to `log/catter-hook.log` in the catter data directory)
2. Is ready to intercept -- the `Executor` lazily reads session state from the environment on first interception

### Environment Variables

The hook reads session information from these environment variables, set by the proxy when it launches the build command:

| Variable | Purpose |
|----------|---------|
| `__key_catter_proxy_path_v1` | Absolute path to the `catter-proxy` binary |
| `__key_catter_command_id_v1` | Session ID of the parent process |

### Interception Flow

When any process creation function is called (e.g., `execve("/usr/bin/g++", argv, envp)`):

1. The hook's replacement function is entered.

2. **`Executor`** validates the session. If the session is invalid (missing environment variables), it builds an error command that reports the problem to the daemon.

3. **`Resolver`** resolves the target executable to an absolute path. For functions like `execvp()` and `execvpe()`, it searches directories in `PATH`. For `execve()`, it resolves relative to the current directory.

4. **`CmdBuilder`** constructs the proxy command:
   ```
   <proxy_path> -p <self_id> --exec <resolved_path> -- <original_argv...>
   ```
   The original `argv[0]` and all subsequent arguments are preserved after the `--` separator.

5. **`EnvGuard`** (RAII) modifies the environment array:
   - Removes `__key_catter_proxy_path_v1` and `__key_catter_command_id_v1`
   - Strips the hook library name from `LD_PRELOAD` (or `DYLD_INSERT_LIBRARIES`)
   - If `LD_PRELOAD` becomes empty after stripping, removes it entirely

6. **`Linker`** calls the **real** `execve()` (obtained via `dlsym(RTLD_NEXT, ...)`) with the rewritten command.

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
2. **API hooking via MinHook** -- to intercept `CreateProcess` calls within those processes

### Hook DLL

The hook is compiled as `catter-hook-win64.dll`. It uses MinHook, a lightweight x86/x64 API hooking library that works by overwriting the first few bytes of a target function with a jump to a detour function (trampoline hooking).

### Environment Variables

| Variable | Purpose |
|----------|---------|
| `CATTER_IPC_ID` | Session ID of the parent process |
| `CATTER_PROXY_PATH` | Absolute path to the `catter-proxy.exe` binary |

### DLL Injection Process

When `catter-proxy` (in injector mode) needs to start a build command with the hook attached:

1. **Create the target process suspended**. The proxy calls `CreateProcessA()` with the `CREATE_SUSPENDED` flag. The process is created but its main thread does not run.

2. **Set environment variables**. `CATTER_IPC_ID` and `CATTER_PROXY_PATH` are set in the target process environment before creation (passed via the environment block).

3. **Allocate memory in the target process**. The proxy calls `VirtualAllocEx()` to allocate a region of memory in the target process's address space.

4. **Write the DLL path**. The proxy calls `WriteProcessMemory()` to write the full path of `catter-hook-win64.dll` into the allocated memory.

5. **Create a remote thread to load the DLL**. The proxy tries three methods in order:
   - `CreateRemoteThread()` -- Documented Win32 API. Tried first as it is the most widely supported.
   - `NtCreateThreadEx()` -- Undocumented NT API. Fallback that works reliably on modern Windows (Vista+).
   - `RtlCreateUserThread()` -- Another undocumented NT API. Last resort fallback.

   The remote thread's entry point is `LoadLibraryA`, and its argument is the pointer to the DLL path string written in step 4. When the thread runs, it loads `catter-hook-win64.dll` into the target process.

6. **Wait for injection to complete**. The proxy waits for the remote thread to finish (with a 3-second timeout).

7. **Resume the main thread**. The proxy calls `ResumeThread()` to let the target process start executing. By this point, the hook DLL is loaded and its hooks are active.

### Hooked Windows APIs

The hook DLL installs MinHook trampoline hooks on these functions during `DLL_PROCESS_ATTACH`:

- **`CreateProcessA()`** -- ANSI version of process creation
- **`CreateProcessW()`** -- Wide-character version of process creation
- **`CreateProcessAsUserA()`** -- Passthrough (currently not rewritten)
- **`CreateProcessAsUserW()`** -- Passthrough (currently not rewritten)

MinHook works by:
1. Saving the first few instructions of the target function
2. Overwriting them with a jump to the detour function
3. Providing a trampoline that contains the saved instructions followed by a jump back, so the original function can still be called

### Hook Detour Logic

When `CreateProcessA` or `CreateProcessW` is called by the hooked process:

1. **Extract the command line**. The hook reads `lpApplicationName` and `lpCommandLine`.

2. **Resolve the absolute path**. The hook resolves the executable using `resolve_abspath()`, which searches the program directory, current directory, System32, Windows directory, and `PATH` (matching the standard Windows search order).

3. **Rewrite the command line**:
   ```
   {proxy_path} -p {ipc_id} --exec {resolved_path} -- {original_cmdline}
   ```
   The `lpApplicationName` is set to `nullptr` so that the command line is parsed by `CreateProcess` normally.

4. **Call the original `CreateProcess`** via the MinHook trampoline with the modified command line.

### Key Difference from Unix

On Unix, `LD_PRELOAD` is an environment variable inherited automatically by all child processes. The hook is "viral" by default -- every subprocess gets it.

On Windows, there is no such mechanism. The hook DLL must be explicitly injected into each new process. This happens because:

1. The hook's `CreateProcess` detour intercepts every process creation
2. It rewrites the command to go through `catter-proxy`
3. When the proxy decides to execute with `INJECT` action, it performs DLL injection into the new process (create suspended, inject, resume)

This means the proxy is responsible for re-injecting the hook into each generation of child processes, maintaining the recursive interception chain.

### DLL Lifecycle

```
DLL_PROCESS_ATTACH:
  MH_Initialize()       -- Initialize MinHook
  MH_CreateHook(...)    -- Install hooks on CreateProcess variants
  MH_EnableHook(...)    -- Activate all hooks

DLL_PROCESS_DETACH:
  MH_DisableHook(...)   -- Deactivate all hooks
  MH_Uninitialize()     -- Clean up MinHook
```

`DisableThreadLibraryCalls()` is called during attach to suppress `DLL_THREAD_ATTACH` and `DLL_THREAD_DETACH` notifications, reducing overhead.

## Platform Comparison

| Aspect | Linux | macOS | Windows |
|--------|-------|-------|---------|
| Hook library | `libcatter-hook-unix.so` | `libcatter-hook-unix.dylib` | `catter-hook-win64.dll` |
| Injection method | `LD_PRELOAD` env var | `DYLD_INSERT_LIBRARIES` env var | `VirtualAllocEx` + `LoadLibraryA` |
| Interposition | `dlsym(RTLD_NEXT, ...)` | `DYLD_INTERPOSE` macro | MinHook trampoline hooking |
| Intercepted APIs | `execve`, `execvp`, `posix_spawn`, etc. | Same as Linux | `CreateProcessA`, `CreateProcessW` |
| Recursive hooking | Automatic (env inherited) | Automatic (env inherited) | Explicit (DLL re-injected per process) |
| Env vars for session | `__key_catter_proxy_path_v1`, `__key_catter_command_id_v1` | Same as Linux | `CATTER_PROXY_PATH`, `CATTER_IPC_ID` |
