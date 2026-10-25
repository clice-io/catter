# Handler protocol

This page specifies the protocol between catter and a [handler](../scripting/handlers.md), for writing a handler without an SDK. The SDKs implement it, with types generated from the same definition catter's are.

## Transport

Messages are [JSON-RPC 2.0](https://www.jsonrpc.org/specification), each framed as the Language Server Protocol frames them: a `Content-Length: <n>` header, an empty line (`\r\n\r\n`), then `n` bytes of UTF-8 JSON.

- A handler catter starts (`--handler`) reads catter's messages on its stdin and writes its own on its stdout. Its stderr is the terminal's.
- A handler that listens (`--connect`) does the same over each TCP connection: one connection is one session.

catter sends requests and notifications; a handler answers the requests and sends nothing else. Beside the messages below, catter may send `$/cancelRequest`, as the Language Server Protocol does, for a request it no longer waits for: a handler may ignore it.

## A session

1. `initialize` (request): the session starts. The handler answers with what it wants to be told.
2. While the build runs, for each command: `command`, a request to a handler that decides and a notification to one that does not; `error` when catter could not capture a command; `exit` once a command has run. Notifications arrive only if the handler asked for them.
3. `finish` (request): the build has ended. Once the handler has answered, catter ends its input: a handler it started then exits; over TCP, catter shuts down its side of the connection, and the handler closes it.

Commands of a parallel build overlap: `command` requests can be in flight together, and are answered by their ids, in any order.

## Messages

### `initialize`

| Field           | Type            | Description                                           |
| --------------- | --------------- | ----------------------------------------------------- |
| `version`       | integer         | The protocol's version: `1`.                          |
| `session`       | string          | Tells apart the sessions one handler serves over TCP. |
| `mode`          | string          | How commands are captured: `inject`, or `replay`.     |
| `build.command` | OS string array | The build command.                                    |
| `build.cwd`     | OS string       | Its working directory.                                |

The result:

| Field            | Type    | Description                                                   |
| ---------------- | ------- | ------------------------------------------------------------- |
| `decides`        | boolean | Every command comes as a request, which the build waits for.  |
| `events.command` | boolean | A handler that does not decide is told of each command.       |
| `events.error`   | boolean | The handler is told of the commands catter could not capture. |
| `events.exit`    | boolean | The handler is told of each command's exit.                   |
| `env`            | boolean | Commands carry their environment.                             |

### `command`

| Field        | Type                      | Description                                                  |
| ------------ | ------------------------- | ------------------------------------------------------------ |
| `id`         | integer                   | The command's id in the session.                             |
| `parent`     | integer, optional         | The command that started it; absent for the build command.   |
| `cwd`        | OS string                 | Its working directory.                                       |
| `executable` | OS string                 | The program it runs.                                         |
| `args`       | OS string array           | Its arguments, the program's name first.                     |
| `env`        | OS string array, optional | Its environment, `NAME=value` entries, if the handler asked. |

The result of the request is an action, named by its `action` field:

| `action` | Fields                                            | What catter does                                                                       |
| -------- | ------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `skip`   |                                                   | Runs the command as it is.                                                             |
| `drop`   |                                                   | Does not run it: it exits 0.                                                           |
| `abort`  | `reason`: string                                  | Ends the session (below), reporting the reason.                                        |
| `modify` | `args`, and optionally `executable`, `cwd`, `env` | Runs the command with these in place of its own; a field left out keeps the command's. |

### `error`, `exit` and `finish`

| Message  | Fields                                                                                                                                                                                               |
| -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `error`  | `id`, `parent` (optional), `message`: why catter could not capture the command.                                                                                                                      |
| `exit`   | `id`, `code`: its exit code, `duration`: microseconds from the handler's decision on the command (its notification, to an observer) to its exit, in which the handlers after it in the chain decide. |
| `finish` | `code`: the build's exit code. The result is `null`.                                                                                                                                                 |

## Strings and numbers

- **OS strings.** An argument, a path or an environment entry is a JSON string when its bytes are valid UTF-8, and `{"base64": "<the bytes in base64>"}` otherwise: on Linux and macOS they are bytes, which need not be UTF-8. A handler hands such a value back as it got it, or builds one the same way.
- Every other string is valid Unicode. JSON text has no byte order mark and no object repeats a key.
- An optional field that is absent may be left out or be `null`.
- Every integer of version 1 is below 2<sup>53</sup>, which a JavaScript number holds exactly.

## Errors

A handler that cannot answer a request answers with a JSON-RPC error; its `message` is what catter reports. catter ends the session when a handler answers with an error or with `abort`, stops (its output ends), writes what is not an answer to one of its requests, or does not answer within `--handler-timeout`: the command being decided, and every command the build starts after it, fail without running; once the build has ended, catter ends the handlers' input without a `finish`, prints the reason and exits 1.

An observer cannot answer a notification: one that fails exits, which catter reports when it next writes to it.
