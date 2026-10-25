# Runtime

A runtime is the mechanism by which catter actually captures process creation. Catter is organized around runtimes: handlers do not care how commands are captured, only what the current runtime provides.

## Why multiple runtimes

"Intercepting process creation" differs greatly in feasibility and cost across platforms:

- **Hooks are the most general**, transparently intercepting any child process creation, but they depend on platform features. For example, macOS restricts `DYLD_INSERT_LIBRARIES` and requires special development entitlements; Windows has no preload mechanism and relies on DLL injection instead.
- **Some scenarios do not need hooks at all**. If a build system honors the `CC`/`CXX` environment variable conventions, replacing the compiler is enough to capture every compilation command.
- **Some platforms provide dedicated monitoring APIs**, such as macOS Event Stream / Endpoint Security, which can replace or complement hooks.

For these reasons catter abstracts capture mechanisms as runtimes: one runtime is one capture implementation, with the capabilities listed below. Handlers adapt to them instead of assuming a particular runtime exists.

## Mechanisms

### inject (implemented, default)

- **How it works**: a hook shared library is injected into the build system process. It intercepts process creation calls (the `execve` / `posix_spawn` family on Unix, `CreateProcess` on Windows) and rewrites commands so they run through `catter-proxy`.
- **Platforms**: Linux, macOS, Windows.
- **Capabilities**: every action (`skip`, `drop`, `abort`, `modify`); commands carry a parent command ID.
- **Strengths**: recursive capture -- every descendant process is intercepted; captured commands carry the full environment and a parent command ID. The build and its commands run on catter's own stdin, stdout and stderr: they write straight to the terminal, with their colors and in the order they write.
- **Limitations**: the implementation relies on platform features (preload / DLL injection). On macOS, `DYLD_INSERT_LIBRARIES` is restricted by the system and requires special development entitlements; on Windows, DLL injection may conflict with strong signing or security software policies.
- **Implementation details**: [Hook mechanism](hook-mechanism.md), [IPC protocol](ipc-protocol.md).

### env (planned)

- **How it works**: sets `CC` / `CXX` and similar environment variables to redirect compiler invocations to `catter-proxy` (wrapper mode).
- **Use cases**: build systems that honor the standard compiler environment variable conventions can be captured without hooks.
- **Expected limitations**: only invocations routed through those environment variables are captured; hard-coded compiler paths and build scripts with inline `cc` may bypass it; parent command IDs are expected to be unavailable.
- **Status**: not implemented yet. Exact capabilities (such as the actions it supports) will be confirmed when implemented.

### eslogger (planned)

- **How it works**: uses the macOS Event Stream monitoring API to observe process creation / execution events (side-channel monitoring).
- **Use cases**: an alternative on macOS when hooks are restricted by permissions.
- **Expected limitations**: macOS only; requires a system-granted monitoring entitlement; how far side-channel monitoring can intervene in execution (`modify` / `drop`, for example) is to be confirmed when implemented.
- **Status**: not implemented yet.

## Capability matrix

| Mechanism | Platforms               | Capture method                     | Actions                        | Parent command ID | Status                |
| --------- | ----------------------- | ---------------------------------- | ------------------------------ | ----------------- | --------------------- |
| inject    | Linux / macOS / Windows | hook intercepting process creation | `skip` `drop` `abort` `modify` | yes               | implemented (default) |
| env       | cross-platform          | `CC` / `CXX` environment variables | TBD                            | expected no       | planned               |
| eslogger  | macOS                   | Event Stream API                   | TBD                            | TBD               | planned               |

## Selecting and checking

- catter captures a build with `inject`, which requires no configuration; `--from` plays a recorded build instead of capturing one.
- Handlers should not assume a mechanism: `initialize` names the current one (`mode`).
- Under a runtime whose commands carry no parent command ID, a command's `parent` will be absent, and features relying on parent-child relationships (command trees, target trees) need to degrade gracefully.
