# IPC Protocol

Catter uses inter-process communication between the daemon (`catter`) and proxy instances (`catter-proxy`). The daemon acts as a server, and each proxy instance connects as a client.

## Transport

The transport layer is platform-specific:

| Platform | Mechanism | Path / Name |
|----------|-----------|-------------|
| Linux / macOS | Unix domain socket | `$XDG_DATA_HOME/pipe-catter-ipc.sock` (typically `~/.local/share/pipe-catter-ipc.sock`) |
| Windows | Named pipe | `\\.\pipe\catter-ipc` |

The daemon creates the listening socket/pipe at startup. Each `catter-proxy` instance connects to it as a client when it starts. The connection persists for the lifetime of the proxy process.

## Serialization

All messages are encoded using [Bincode](https://github.com/bincode-org/bincode), a compact binary serialization format. The implementation uses `kota::ipc::BincodePeer`, which provides a request-response abstraction over a stream transport (`kota::ipc::StreamTransport`).

Bincode was chosen for its efficiency -- messages are small and fast to encode/decode, which matters because every intercepted build command results in multiple IPC round-trips.

## Request-Response Model

Communication follows a strict request-response pattern. The client (proxy) sends a request and waits for the server (daemon) to respond before proceeding. Each request type has a well-defined parameter type and result type.

The protocol is defined in `src/common/util/data.h` using C++ template specialization:

```cpp
enum class RequestType : uint8_t {
    CHECK_MODE,
    CREATE,
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
};
```

Each request type maps to a `Request<Type>` specialization that declares `Params` (the request payload) and `Result` (the response payload).

## Request Types

### CHECK_MODE

Asks the daemon what service mode is active.

| Field | Type | Description |
|-------|------|-------------|
| **Params** | `ServiceMode` (enum) | The mode to check (currently only `INJECT`) |
| **Result** | `bool` | `true` if the daemon is in the requested mode |

This is the first request a proxy sends after connecting. It confirms the daemon is ready to handle intercepted commands.

### CREATE

Registers a new process session with the daemon.

| Field | Type | Description |
|-------|------|-------------|
| **Params** | `ipcid_t` (`int32_t`) | The parent session ID |
| **Result** | `ipcid_t` (`int32_t`) | A unique session ID assigned to this proxy instance |

The parent ID establishes the parent-child relationship in the session tree. For the first proxy (injector mode), the parent ID is provided by the daemon when it spawns the proxy. For wrapper-mode proxies, the parent ID comes from the `__key_catter_command_id_v1` environment variable (or `CATTER_IPC_ID` on Windows), which was set by the hook.

### MAKE_DECISION

The core request. The proxy sends a captured command and the daemon decides what to do with it.

**Params** -- `command`:

| Field | Type | Description |
|-------|------|-------------|
| `cwd` | `string` | Working directory of the intercepted process |
| `executable` | `string` | Resolved absolute path to the executable |
| `args` | `string[]` | Full argument array (including `argv[0]`) |
| `env` | `string[]` | Environment variables (in `KEY=VALUE` format) |

**Result** -- `action`:

| Field | Type | Description |
|-------|------|-------------|
| `type` | `uint8_t` enum | One of `DROP`, `INJECT`, or `WRAP` |
| `cmd` | `command` | The command to execute (may be modified by the script) |

**Action types**:

- **`DROP` (0)** -- Do not execute the command. The proxy returns exit code 0 immediately. Used when the script determines a command is irrelevant (e.g., a compiler invocation the user wants to skip).
- **`INJECT` (1)** -- Execute the command with the hook library attached. The proxy re-adds `LD_PRELOAD` (or performs DLL injection on Windows) so that child processes of this command are also intercepted. This is the default for build commands whose children should be monitored.
- **`WRAP` (2)** -- Execute the command directly without hooking. The proxy runs the command and captures its stdout/stderr, but does not inject the hook. Used for leaf commands (like actual compiler invocations) that do not spawn further build processes.

The daemon may modify the command in the returned action. For example, a script could change compiler flags, redirect output paths, or substitute a different executable.

### REPORT_ERROR

Reports an error condition from the hook or proxy back to the daemon.

**Params**:

| Field | Type | Description |
|-------|------|-------------|
| `parent_id` | `ipcid_t` | Session ID of the parent process |
| `error_msg` | `string` | Human-readable error description |

**Result**: `null` (no response payload)

This is used when the hook encounters an invalid state (e.g., missing environment variables) or when the proxy catches an exception during command processing. The daemon logs the error and can notify the user.

### FINISH

Reports that a command has completed execution.

**Params** -- `process_result`:

| Field | Type | Description |
|-------|------|-------------|
| `code` | `int64_t` | Process exit code |
| `std_out` | `string` | Captured standard output |
| `std_err` | `string` | Captured standard error |

**Result**: `null` (no response payload)

After the daemon receives this request, it invokes the `onExecution()` JavaScript callback with the result data. The proxy then disconnects.

## Typical Message Sequence

A complete proxy lifecycle involves this sequence of IPC messages:

### Injector Mode (first proxy)

```
Proxy -> Daemon:  CHECK_MODE(INJECT)
Daemon -> Proxy:  true
[Proxy launches build command with hook attached]
[Proxy waits for build to complete]
[Proxy exits when build finishes]
```

The injector does not send `CREATE`, `MAKE_DECISION`, or `FINISH` -- it simply launches the build and waits.

### Wrapper Mode (intercepted command)

```
Proxy -> Daemon:  CHECK_MODE(INJECT)
Daemon -> Proxy:  true
Proxy -> Daemon:  CREATE(parent_id)
Daemon -> Proxy:  new_session_id
Proxy -> Daemon:  MAKE_DECISION(command)
Daemon -> Proxy:  action {type, cmd}
[Proxy executes or drops the command]
Proxy -> Daemon:  FINISH(process_result)
Daemon -> Proxy:  null
[Proxy disconnects]
```

### Error Case

```
Proxy -> Daemon:  REPORT_ERROR(parent_id, error_message)
Daemon -> Proxy:  null
[Proxy exits with code -1]
```

## Session Model

The daemon maintains a session tree that mirrors the process tree of the build:

```
Session 0 (root -- injector proxy)
  +-- Session 1 (make -> gcc file1.c)
  +-- Session 2 (make -> gcc file2.c)
  +-- Session 3 (make -> ar rcs libfoo.a file1.o file2.o)
  +-- Session 4 (make -> sh -c "gcc file3.c")
       +-- Session 5 (sh -> gcc file3.c)
```

Each session is identified by a unique `ipcid_t` (32-bit integer). The tree is built incrementally as `CREATE` requests arrive, each specifying a `parent_id`. This structure enables:

- **Target tree reconstruction** -- by knowing which object files are linked into which targets
- **Build profiling** -- by tracking timing data per session and visualizing the parallelism
- **Error attribution** -- by tracing errors back to the build command that caused them

## Connection Lifecycle

1. **Daemon starts** -- Creates and binds the Unix domain socket or named pipe.
2. **Proxy connects** -- Opens a connection to the socket/pipe. The connection is established using `kota::pipe::connect()` on the proxy side.
3. **Daemon accepts** -- For each incoming connection, the daemon creates a `kota::ipc::BincodePeer` and registers request handlers (`on_request<Request<Type>>(...)`) for each request type.
4. **Request handling** -- The daemon's event loop dispatches incoming requests to the appropriate handler. Handlers may be async (using `co_await`) to interact with the JS runtime or other daemon state.
5. **Peer disconnects** -- When the proxy process exits, the transport closes and the `BincodePeer::run()` coroutine completes. The daemon logs the disconnection and cleans up the session.

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
    std::string std_out;               // Captured stdout
    std::string std_err;               // Captured stderr
};

// Decision returned by the daemon
struct action {
    enum : uint8_t {
        DROP,    // Do not execute
        INJECT,  // Execute with hook attached
        WRAP,    // Execute without hook
    } type;
    command cmd;                       // Possibly modified command
};
```
