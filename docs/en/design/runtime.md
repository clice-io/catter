# Runtime

A runtime is the mechanism by which catter actually captures process creation. Catter is organized around runtimes: scripts do not care how commands are captured, only what capabilities the current runtime provides.

## Why multiple runtimes

"Intercepting process creation" differs greatly in feasibility and cost across platforms:

- **Hooks are the most general**, transparently intercepting any child process creation, but they depend on platform features. For example, macOS restricts `DYLD_INSERT_LIBRARIES` and requires special development entitlements; Windows has no preload mechanism and relies on DLL injection instead.
- **Some scenarios do not need hooks at all**. If a build system honors the `CC`/`CXX` environment variable conventions, replacing the compiler is enough to capture every compilation command.
- **Some platforms provide dedicated monitoring APIs**, such as macOS Event Stream / Endpoint Security, which can replace or complement hooks.

For these reasons catter abstracts capture mechanisms as runtimes: one runtime is one capture implementation that exposes a fixed capability description. Scripts adapt to those capabilities instead of assuming a particular runtime exists.

## Capability model

The `config.runtime` object passed to `onStart` describes the current runtime:

| Field | Meaning |
|-------|---------|
| `type` | Runtime type: `inject` / `eslogger` / `env` |
| `supportActions` | Supported script actions (a subset of `skip` / `drop` / `abort` / `modify`) |
| `supportParentId` | Whether captured commands can report a parent command ID (used by command trees, target trees, etc.) |

## Mechanisms

### inject (implemented, default)

- **How it works**: a hook shared library is injected into the build system process. It intercepts process creation calls (the `execve` / `posix_spawn` family on Unix, `CreateProcess` on Windows) and rewrites commands so they run through `catter-proxy`.
- **Platforms**: Linux, macOS, Windows.
- **Capabilities**: `supportActions = [skip, drop, modify]`, `supportParentId = true`.
- **Strengths**: recursive capture -- every descendant process is intercepted; captured commands carry the full environment and a parent command ID.
- **Limitations**: the implementation relies on platform features (preload / DLL injection). On macOS, `DYLD_INSERT_LIBRARIES` is restricted by the system and requires special development entitlements; on Windows, DLL injection may conflict with strong signing or security software policies.
- **Implementation details**: [Hook mechanism](hook-mechanism.md), [IPC protocol](ipc-protocol.md).

### env (planned)

- **How it works**: sets `CC` / `CXX` and similar environment variables to redirect compiler invocations to `catter-proxy` (wrapper mode).
- **Use cases**: build systems that honor the standard compiler environment variable conventions can be captured without hooks.
- **Expected limitations**: only invocations routed through those environment variables are captured; hard-coded compiler paths and build scripts with inline `cc` may bypass it; parent command IDs are expected to be unavailable.
- **Status**: not implemented yet. Exact capabilities (such as `supportActions`) will be confirmed when implemented.

### eslogger (planned)

- **How it works**: uses the macOS Event Stream monitoring API to observe process creation / execution events (side-channel monitoring).
- **Use cases**: an alternative on macOS when hooks are restricted by permissions.
- **Expected limitations**: macOS only; requires a system-granted monitoring entitlement; how far side-channel monitoring can intervene in execution (`modify` / `drop`, for example) is to be confirmed when implemented.
- **Status**: not implemented yet.

## Capability matrix

| Mechanism | Platforms | Capture method | supportActions | supportParentId | Status |
|-----------|-----------|----------------|----------------|-----------------|--------|
| inject | Linux / macOS / Windows | hook intercepting process creation | `skip` `drop` `modify` | yes | implemented (default) |
| env | cross-platform | `CC` / `CXX` environment variables | TBD | expected no | planned |
| eslogger | macOS | Event Stream API | TBD | TBD | planned |

## Selecting and checking

- `inject` is the default and requires no configuration; other mechanisms will be selectable via `-m/--mode` once available.
- Scripts should not assume a mechanism: check `config.runtime.supportActions` in `onStart` and only use actions the current runtime supports.
- When `supportParentId = false`, `CommandData.parent` will be absent, and features relying on parent-child relationships (command trees, target trees) need to degrade gracefully.
