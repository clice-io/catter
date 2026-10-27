# IPC Protocol (hook and wrapper backends)

> This page describes the communication protocol between the catter daemon and what reports the build's commands to it: `catter-proxy`, which the hook runs each command through on Linux and macOS and which the wrapper backend's shims are, and the hook itself on Windows. The protocol names no backend: it is what every backend that runs a build reports its commands over; handler authors usually do not need to read it.

Catter uses inter-process communication between the daemon (`catter`) and its clients: each `catter-proxy` instance, which stands in for one command; on Windows the hook too, from inside the `CreateProcess` call that starts one command. The daemon acts as a server, and each client connects once per command.

## Transport

The transport layer is platform-specific:

| Platform      | Mechanism          | Path / Name                 |
| ------------- | ------------------ | --------------------------- |
| Linux / macOS | Unix domain socket | `<data dir>/ipc-<pid>.sock` |
| Windows       | Named pipe         | `\\.\pipe\catter-ipc-<pid>` |

Each daemon listens on a socket/pipe of its own, named by its process id, so builds that run under catter at the same time never meet. The data directory is `$HOME/.catter` (Linux, macOS) or `%LOCALAPPDATA%\catter` (Windows), or `CATTER_HOME` when it is set. The daemon creates the listening socket/pipe at startup. Every `catter-proxy` finds its name in its environment (`__key_catter_ipc_v1`), which the daemon sets for the build command, and the hook, or a shim of the wrapper backend, for each command after it. Each one connects to it as a client when it starts, and the connection persists for the lifetime of the proxy process. On Windows the daemon gives it to the hook in each process it injects the hook into; the hook connects for each program it starts, and disconnects once the program is started.

A program can outlive the session it started in: a build server (an sccache server, a Gradle daemon) runs the commands of later builds with the earlier session's pipe in its environment, or, on Windows, its hook. When that pipe is gone, the command runs as it would run without catter, uncaptured; `catter-proxy` logs a warning. A command that ends after its session keeps its exit code. A command whose working directory the proxy cannot read, as one the build removed, runs there all the same, uncaptured, and the proxy reports it as an error (`REPORT_ERROR`).

`catter-proxy` stands in for the program it starts, and the build sees the program's end as the proxy's: a program a signal ended ends the proxy by the same signal. The program starts with the signals as exec would leave them: those the build ignores stay ignored, those it blocks blocked. A standard stream the build closed stays closed for the program: nothing the proxy opens takes its place. A SIGTERM, SIGINT or SIGHUP another process sends the proxy goes on to the program; one the terminal sends reaches the program itself. A program ends with its proxy when the proxy is killed: on Linux the system ends it; macOS has no way to tie the two, and there the daemon ends it, told the program's process id (`started`), when the proxy disconnects before its `FINISH`.

The daemon stands in for the build the same way: it starts the build command's proxy as a proxy starts its program, and ends as the build did. A SIGTERM, SIGINT or SIGHUP sent to the daemon goes on to the build command, and the proxy of the build command ends with the daemon when the daemon is killed: on macOS that proxy ends its program itself when its connection to the daemon ends before its `FINISH`.

The hook on Windows runs no proxy: the daemon decides the build command itself and starts it with the hook, and the hook starts each program the build starts. The caller gets the program's own process, as it would without catter, and the daemon waits on the process for its exit code. The daemon stands in for the build command: Ctrl+C and Ctrl+Break reach the build in the console they share, and the daemon waits for it to end; the build command ends with the daemon when the daemon is killed.

## Serialization

All messages are encoded using [Bincode](https://github.com/bincode-org/bincode), a compact binary serialization format, each framed by a `Content-Length` header. The daemon and `catter-proxy` use `kota::ipc::BincodePeer`, which provides a request-response abstraction over a stream transport (`kota::ipc::StreamTransport`); the Windows hook, which runs inside the build's processes and has no event loop, sends the same frames through `kota::ipc::BlockingClient`, which waits for each answer.

Bincode was chosen for its efficiency -- messages are small and fast to encode/decode, which matters because every intercepted build command results in multiple IPC round-trips.

## Request-Response Model

Communication follows a strict request-response pattern, but for `started` on macOS, a notification. The client sends a request and waits for the server (daemon) to respond before proceeding. Each request type has a well-defined parameter type and result type.

The protocol is defined in `src/support/messages.h`, which the daemon, `catter-proxy` and the Windows hook share, using C++ template specialization:

```cpp
enum class RequestType : uint8_t {
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
    STARTED,
};
```

Each request type maps to a `Request<Type>` specialization that declares `Params` (the request payload) and `Result` (the response payload).

## Request Types

### MAKE_DECISION

The core request, which opens each command. The client sends a captured command and the command that started it, and the daemon gives the command its ID and decides what to do with it.

**Params**:

| Field     | Type                | Description                                                       |
| --------- | ------------------- | ----------------------------------------------------------------- |
| `parent`  | `optional<ipcid_t>` | The ID of the command that started it; none for the build command |
| `command` | `command`           | The captured command                                              |

The parent establishes the parent-child relationship in the session tree. The proxy of a command the hook intercepted sends the ID of the command whose process started it, which the hook passes on in the `__key_catter_command_id_v1` environment variable; a shim of the wrapper backend sends the ID in the same variable, which the shim, or the launcher, that started the program it was run from set to its own command's ID. The proxy of the build command, and the launcher of the build command, find none and send none. On Windows the hook sends the ID of the command its process runs, which the daemon gave it.

`command`:

| Field        | Type       | Description                                   |
| ------------ | ---------- | --------------------------------------------- |
| `cwd`        | `string`   | Working directory of the intercepted process  |
| `executable` | `string`   | Resolved absolute path to the executable      |
| `args`       | `string[]` | Full argument array (including `argv[0]`)     |
| `env`        | `string[]` | Environment variables (in `KEY=VALUE` format) |

**Result**:

| Field    | Type      | Description                                                                     |
| -------- | --------- | ------------------------------------------------------------------------------- |
| `id`     | `ipcid_t` | The ID the daemon gives the command, which those it starts name as their parent |
| `action` | `action`  | What to do with the command                                                     |

`action`:

| Field  | Type           | Description                                                        |
| ------ | -------------- | ------------------------------------------------------------------ |
| `type` | `uint8_t` enum | One of `DROP`, `RUN`, or `FAIL`                                    |
| `cmd`  | `command`      | The command to execute: the one sent, unless a handler modified it |

**Action types**:

- **`DROP` (0)** -- Do not execute the command: it ends with exit code 0. Used when a handler determines a command is irrelevant (e.g., a compiler invocation the user wants to skip). The proxy returns exit code 0 immediately; on Windows the daemon injects the hook into the program the hook started suspended, set to end the process with exit code 0 as soon as it loads, before the program's own code runs.
- **`RUN` (1)** -- Execute the command. The proxy runs it with the hook library attached, re-adding `LD_PRELOAD`, or on Windows the daemon injects the hook DLL into the program the hook started, so that child processes of this command are also intercepted, but for a program the hook cannot load (on macOS Apple's own tools and programs under Rosetta, on Windows a machine with no hook DLL), which runs without it. This is what a command gets unless a handler drops it or the session has ended with an error.
- **`FAIL` (2)** -- Do not execute the command: it ends with exit code 1, as for `DROP`. The session has ended with an error (a handler aborted the build, or failed), and every command that starts after it fails this way until the build ends.

The daemon may modify the command in the returned action. For example, a handler could change compiler flags, redirect output paths, or substitute a different executable.

> These actions belong to the **protocol layer** and are not the same layer as the `skip` / `drop` / `abort` / `modify` actions handlers decide: the session maps handler actions to protocol actions. See the Actions section of [System Architecture](architecture.md) for the mapping and semantics.

### REPORT_ERROR

Reports an error condition from the hook or proxy back to the daemon.

**Params**:

| Field     | Type                | Description                                                   |
| --------- | ------------------- | ------------------------------------------------------------- |
| `parent`  | `optional<ipcid_t>` | The ID of the command that started it, as for `MAKE_DECISION` |
| `message` | `string`            | Human-readable error description                              |

**Result**: `null` (no response payload)

This is used when the proxy cannot capture its command (a shim finds no program past the shims, the working directory cannot be read) or catches an exception during command processing, or, on Windows, when the program the hook was to start cannot start. The daemon logs the error and can notify the user.

### FINISH

Reports that a command has completed execution.

**Params** -- `process_result`:

| Field      | Type      | Description                                                           |
| ---------- | --------- | --------------------------------------------------------------------- |
| `code`     | `int64_t` | Process exit code; 128 and the signal's number for one a signal ended |
| `duration` | `int64_t` | How long the program ran, in microseconds                             |

**Result**: `null` (no response payload)

After the daemon receives this request, it tells the session's handlers the result (the handler protocol's `exit`). The proxy then disconnects. The Windows hook does not send it: the daemon learns the exit code from the process itself.

### STARTED

Tells the daemon the process id of the command's program.

**Params**:

| Field | Type      | Description              |
| ----- | --------- | ------------------------ |
| `pid` | `int64_t` | The program's process id |

**Result**: `bool` -- whether the program runs as decided

On Windows the hook sends it once it has started the program, suspended. The daemon opens the process and makes it run as decided: it injects the hook (`RUN`), or injects it set to end the process with exit code 0 (`DROP`) or 1 (`FAIL`) as soon as it loads, so that a caller that prepares the process while it is suspended still can; a process of a machine catter has no hook for, x86, it ends at once. Then it answers `true`, and the hook resumes the program, unless its caller asked for it suspended; the daemon waits for the process to exit, then tells the session's handlers its exit code, as for `FINISH`. When the daemon cannot make the program run as decided, it reports the error to the handlers and answers `false`, and the hook ends the program with exit code 1.

Before it decides a command, and once the build command has ended, the daemon waits until the exits of the programs that have ended are told: the build sees a program end before the daemon does, and the commands it starts next, and its own end, come after that exit, as they do on Linux and macOS, where `catter-proxy` sends its `FINISH` before it exits.

On macOS `catter-proxy` sends it as a notification, without waiting for a response, once the program runs. macOS has nothing to end a child with its parent, as Linux and Windows do: when a proxy disconnects after `started` and before `FINISH`, it was killed, and the daemon kills its program with SIGKILL.

## Typical Message Sequence

A complete client lifecycle involves this sequence of IPC messages:

### Proxy (a command)

```
Proxy -> Daemon:  MAKE_DECISION(parent, command)
Daemon -> Proxy:  {id, action {type, cmd}}
[Proxy starts the command, or drops it]
Proxy -> Daemon:  started(pid)              (macOS, a notification)
[Proxy waits for the command]
Proxy -> Daemon:  FINISH(process_result)
Daemon -> Proxy:  null
[Proxy disconnects]
```

The daemon starts the proxy of the build command, which sends the same messages for the build command, with no parent, so that the build command reaches the handlers like any other command; it runs the build with the hook attached. Under the wrapper backend the daemon starts the build command through the launcher, `catter-proxy` by another name among the shims, which reports it the same way and runs it with the shims on its `PATH`; each shim then sends these messages for the program it stands in for.

### Windows Hook (intercepted command)

```
Hook -> Daemon:   MAKE_DECISION(parent, command)
Daemon -> Hook:   {id, action {type, cmd}}
[Hook starts the command suspended]
Hook -> Daemon:   STARTED(pid)
[Daemon injects the hook]
Daemon -> Hook:   true
[Hook resumes the command and disconnects]
[Daemon waits for the command to exit]
```

The daemon decides the build command itself, and starts it with the hook.

### Error Case

```
Proxy -> Daemon:  REPORT_ERROR(parent, message)
Daemon -> Proxy:  null
[Proxy exits with code -1]
```

## Session Model

The daemon maintains a session tree that mirrors the process tree of the build:

```
Session 1 (root -- the build command)
  +-- Session 2 (make -> gcc file1.c)
  +-- Session 3 (make -> gcc file2.c)
  +-- Session 4 (make -> ar rcs libfoo.a file1.o file2.o)
  +-- Session 5 (make -> sh -c "gcc file3.c")
       +-- Session 6 (sh -> gcc file3.c)
```

Each session is identified by a unique `ipcid_t` (32-bit integer). The tree is built incrementally as `MAKE_DECISION` requests arrive, each naming its `parent`. This structure enables:

- **Target tree reconstruction** -- by knowing which object files are linked into which targets
- **Build profiling** -- by tracking timing data per session and visualizing the parallelism
- **Error attribution** -- by tracing errors back to the build command that caused them

## Connection Lifecycle

1. **Daemon starts** -- Creates and binds the Unix domain socket or named pipe.
2. **Client connects** -- Opens a connection to the socket/pipe: `kota::pipe::connect()` in the proxy, `CreateFileW` in the Windows hook.
3. **Daemon accepts** -- For each incoming connection, the daemon creates a `kota::ipc::BincodePeer` and registers request handlers (`on_request<Request<Type>>(...)`) for each request type.
4. **Request handling** -- The daemon's event loop dispatches incoming requests to the appropriate handler. Request handlers may be async (using `co_await`), to wait for the session's handlers or other daemon state.
5. **Peer disconnects** -- When the proxy process exits, or the Windows hook has started its program, the transport closes and the `BincodePeer::run()` coroutine completes. The daemon logs the disconnection and cleans up the session; on macOS it kills the program of a proxy that went before its `FINISH`, and on Windows it waits for the program the hook started to exit.

## Data Types

The core data structures used across the IPC boundary:

```cpp
// Unique identifier for an IPC session
using ipcid_t = int32_t;

// Captured command from the build system
struct command {
    std::string cwd;                   // Working directory
    std::string executable;            // Resolved executable path
    std::vector<std::string> args;     // Argument array
    std::vector<std::string> env;      // Environment (KEY=VALUE entries)
};

// Result of executing a command
struct process_result {
    int64_t code = -1;                 // Exit code
    int64_t duration = 0;              // How long it ran, in microseconds
};

// Decision returned by the daemon
struct action {
    enum : uint8_t {
        DROP,    // Do not execute
        RUN,     // Execute
        FAIL,    // Do not execute: the session has ended with an error
    } type;
    command cmd;                       // Possibly modified command
};
```
