# System Architecture

Catter is a build-process interception and analysis tool organized around **runtimes**. A runtime is the mechanism that actually captures process creation: every command invocation made by the build system becomes an event catter can handle, and is executed according to the decision of the session's handlers -- run as-is, modified, dropped, or aborted.

Handler authors only need to understand two things:

1. **What a runtime is, and which one the current session uses** -- covered here and in [Runtime](runtime.md).
2. **The lifecycle of a session** -- its start and end at the build level, and each command before and after it runs.

## The runtime abstraction

"Intercepting process creation" differs greatly in feasibility across platforms: hooks are the most general, but are restricted on some platforms (for example, macOS requires special development entitlements). Catter therefore abstracts capture mechanisms as runtimes. One runtime is one capture implementation; the handlers see the commands it captures, whichever it is.

catter captures a build with `inject`; given `--from`, it replays a recorded build instead of running one. A handler learns which from the session's `initialize` message, whose `mode` is `inject`, or `replay` for a recorded build.

Today `inject` (based on hooks) is the only capture mechanism; `env` and `eslogger` are planned. The principles, platform limitations, and capability differences of each mechanism are covered in [Runtime](runtime.md).

## Handlers

A session's handlers decide on its commands and are told of its events. They are the built-in the command runs, if any (`compile-commands`, `tree`, `targets`), which runs inside catter, and the [handlers](../scripting/handlers.md) given with `--handler` or `--connect`: programs that catter speaks the [handler protocol](handler-protocol.md) to. The handlers form a chain: the built-in first, then the others in the order of the command line.

## Session lifecycle

```mermaid
sequenceDiagram
    participant Handler
    participant Catter as catter daemon
    participant Runtime as Runtime (capture mechanism)
    participant Build as Build system

    Catter->>Handler: initialize (the build, the mode)
    Handler-->>Catter: What it wants to be told
    Catter->>Runtime: Start the build
    loop For each captured command
        Runtime->>Catter: Command event
        Catter->>Handler: command
        Handler-->>Catter: Action (skip / drop / abort / modify / edit)
        Catter->>Runtime: Execute according to action
        Runtime->>Catter: Execution result
        Catter->>Handler: exit
    end
    Runtime-->>Catter: Build process finished
    Catter->>Handler: finish
```

### Build level: initialize / finish

**`initialize`** starts the session, before the build: it carries the build command, its working directory and the runtime. The handler answers with whether it decides on each command, which events it wants to be told, which commands it wants to hear of, and whether commands carry their environment and what catter makes of them.

**`finish`** comes once the build has ended, with the build's exit code. A handler writes its final output here.

### Command level: command / error / exit

**`command`** comes for each captured command before it runs: its working directory, executable, full argument vector, the parent command that started it, and, if the handler asked, its environment and what catter makes of it: the compiler, archiver or tool it runs, and what its arguments say. A handler that decides answers with an action; one that does not is only told, and the build never waits for it.

**`error`** comes instead when catter could not capture a command: the message says why.

**`exit`** comes after each command finishes, with its exit code and how long it took. Handlers can aggregate statistics or failures here.

## Actions: separating handler semantics from execution mechanics

"How to handle a command" is expressed at two layers in catter:

1. **Handler layer** (what handlers decide): `skip` / `drop` / `abort` / `modify` / `edit` -- semantic and independent of the mechanism.
2. **Protocol layer** (what catter-proxy, or on Windows the hook, carries out): `DROP` / `INJECT` / `FAIL` -- the concrete execution mechanics.

The runtime driver maps handler-layer actions to protocol-layer actions. The current `inject` runtime mapping:

| Handler action | Semantics                                 | Protocol mapping                                                     |
| -------------- | ----------------------------------------- | -------------------------------------------------------------------- |
| `skip`         | Execute as-is, catter does not intervene  | `INJECT` (run with hooks mounted; descendants can still be captured) |
| `drop`         | Do not execute; return exit code 0        | `DROP`                                                               |
| `abort`        | Abort the whole build and report an error | `FAIL` (this command and every later one fail without running)       |
| `modify`       | Execute the command with replacement data | `INJECT` (carrying the modified command)                             |
| `edit`         | Execute the command with edited arguments | `INJECT` (carrying the edited command)                               |

Keep in mind:

- **`skip` is not "ignore the whole subtree"**. `skip` only lets this command run as-is; its descendants are still captured and still reach the handlers.
- **`abort` ends the session**, as a handler that fails does: the command, and every command the build starts after it, fail without running, and once the build has ended catter prints the reason and exits 1.

## Components and processes

A single catter run involves several processes and components. They are all part of the runtime implementation, and handler authors usually do not need to know about them:

| Component           | Role                                                                                                                                                                 |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `catter` (daemon)   | Runs the session's handlers, receives commands over IPC and returns decisions                                                                                        |
| `catter-proxy`      | On Linux and macOS: executes commands and relays IPC; has an injector mode (launching the build) and a wrapper mode (standing in for intercepted commands)           |
| hook shared library | The capture carrier of the inject runtime; injected into build processes to intercept process creation; on Windows it asks the daemon itself and starts each program |

They are **implementation details of the inject runtime**; the runtime abstraction itself is the center of the architecture. See [Runtime](runtime.md) and [Hook mechanism](hook-mechanism.md).
