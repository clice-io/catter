# Handler protocol

This page specifies the protocol between catter and a [handler](../scripting/handlers.md), for writing a handler without an SDK. The SDKs implement it, with types generated from the same definition catter's are.

## Transport

Messages are [JSON-RPC 2.0](https://www.jsonrpc.org/specification), each framed as the Language Server Protocol frames them: a `Content-Length: <n>` header, an empty line (`\r\n\r\n`), then `n` bytes of UTF-8 JSON.

- A handler catter starts (`--handler`) reads catter's messages on its stdin and writes its own on its stdout. Its stderr is the terminal's.
- A handler that listens (`--connect`) does the same over each TCP connection: one connection is one session.

catter sends requests and notifications; a handler answers the requests and may send requests of its own, `analyze`, which catter answers (another method with an error), but no notifications. Beside the messages below, catter may send `$/cancelRequest`, as the Language Server Protocol does, for a request it no longer waits for: a handler may ignore it.

## A session

1. `initialize` (request): the session starts. The handler answers with what it wants to be told.
2. While the build runs, for each command: `command`, a request to a handler that decides and a notification to one that does not; `error` when catter could not capture a command; `exit` once a command has run. Notifications arrive only if the handler asked for them.
3. `finish` (request): the build has ended. Once the handler has answered, catter ends its input: a handler it started then exits; over TCP, catter shuts down its side of the connection, and the handler closes it.

Commands of a parallel build overlap: `command` requests can be in flight together, and are answered by their ids, in any order.

## Messages

### `initialize`

| Field           | Type            | Description                                                                                                                                                                                                       |
| --------------- | --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `version`       | integer         | The protocol's version: `1`.                                                                                                                                                                                      |
| `session`       | string          | Tells apart the sessions one handler serves over TCP.                                                                                                                                                             |
| `mode`          | string          | The capture backend: `hook`, `wrapper`, `eslogger`, or `replay`.                                                                                                                                                  |
| `decisions`     | boolean         | Each command waits for the answer of a handler that decides: false under a backend that only observes them, `eslogger`, which refuses such a handler. Under `replay` the answers change nothing, as nothing runs. |
| `build.command` | OS string array | The build command.                                                                                                                                                                                                |
| `build.cwd`     | OS string       | Its working directory.                                                                                                                                                                                            |

The result:

| Field            | Type                    | Description                                                                                                                                                   |
| ---------------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `decides`        | boolean                 | Every command comes as a request, which the build waits for.                                                                                                  |
| `events.command` | boolean                 | A handler that does not decide is told of each command.                                                                                                       |
| `events.error`   | boolean                 | The handler is told of the commands catter could not capture.                                                                                                 |
| `events.exit`    | boolean                 | The handler is told of each command's exit.                                                                                                                   |
| `env`            | boolean                 | Commands carry their environment.                                                                                                                             |
| `analysis`       | boolean                 | Commands carry what catter makes of them (below).                                                                                                             |
| `filter`         | pattern array, optional | The handler is told only of the commands a pattern names, and of their exits; to a handler that decides, the others are as `skip`. Every command when absent. |

A pattern names commands by what catter makes of them:

| Field    | Type                   | Description                                                                                                      |
| -------- | ---------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `kind`   | string                 | The kind of program they run: `compiler`, `archiver` or `tool`.                                                  |
| `names`  | string array, optional | Of compilers and tools, their names (`gcc`, `clang`, `clang-cl`, `msvc`, `nvcc`; `lld-elf`, …). Any when absent. |
| `phases` | string array, optional | Of compilers, the phases of their commands (`preprocess`, `syntax-only`, `compile`, `link`, …). Any when absent. |

A command whose arguments catter cannot read has no phase: a pattern with `phases` does not name it.

### `command`

| Field        | Type                      | Description                                                                                  |
| ------------ | ------------------------- | -------------------------------------------------------------------------------------------- |
| `id`         | integer                   | The command's id in the session.                                                             |
| `parent`     | integer, optional         | The command that started it; absent for the build command.                                   |
| `cwd`        | OS string                 | Its working directory.                                                                       |
| `executable` | OS string                 | The program it runs.                                                                         |
| `args`       | OS string array           | Its arguments, the program's name first.                                                     |
| `env`        | OS string array, optional | Its environment, `NAME=value` entries, if the handler asked.                                 |
| `analysis`   | analysis, optional        | What catter makes of it, if the handler asked; absent for a program catter knows nothing of. |

The result of the request is an action, named by its `action` field:

| `action` | Fields                                            | What catter does                                                                       |
| -------- | ------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `skip`   |                                                   | Runs the command as it is.                                                             |
| `drop`   |                                                   | Does not run it: it exits 0.                                                           |
| `abort`  | `reason`: string                                  | Ends the session (below), reporting the reason.                                        |
| `modify` | `args`, and optionally `executable`, `cwd`, `env` | Runs the command with these in place of its own; a field left out keeps the command's. |
| `edit`   | `edits`: edit array                               | Runs the command with its arguments edited (below).                                    |

### Analysis

What catter makes of a command, named by its `kind`. catter knows a program by its executable's name, perhaps after a target (`x86_64-linux-gnu-gcc`) and before a version (`gcc-13`, `llvm-ar-18`, MacPorts' `clang-mp-18`): `cc` and `c++` are `gcc`, even where they run clang.

- `compiler`: a command of gcc, clang, clang-cl or MSVC's cl. `compiler`; `dialect`, the syntax its arguments are read in (`gcc`, `clang`, or `msvc`, clang's in cl mode too); `phase` and `artifact`, what it makes; `target` (`triple`, `os`, `objectFormat`, `env`, `artifactModel`, `source`: what tells the target); `expansion`, when the command names response files (`@file`) catter read: `files`, those it read, as named, the files they name too, and `args`, the command's arguments with them read in, which the indices below count in; `arguments`; `reads`, each with `path`, `argument` and `index` (where the command names it), `role` (`source` or `link`) and a source's `language`; `writes`, each with `path`, `reads` (the reads it is made from) and `origin`; and `diagnostics`, how catter inferred the files where the arguments leave them open.
- `archiver`: a command of `ar`, `gcc-ar` or `llvm-ar`. `operation`, `modifiers`, `thin`, `archive`, `members`, `reads` and `writes`.
- `tool`: a command of a linker or a librarian, whose arguments catter reads by its option table, `tool`: `lld-elf`, `lld-coff`, `lld-macho`, `lld-mingw`, `lld-wasm`, `llvm-lib` or `llvm-dlltool`; `lld-coff` and `llvm-lib` take options in any case, as `link.exe` and `lib.exe` do. `arguments`, a response file among them as the command names it: catter reads no tool's response files.
- `unparsed`: a command of such a program whose arguments catter cannot read, nvcc's for now. `of`, the kind of program, `name`, `error` and `message`.

Each argument has `option`, the option it is after aliases, by its name with its prefix (`-I` for `/I` too; `<input>` for an input, `<unknown>` for an option the option table does not know, with the value after it for those of GCC and cl that take one: `-dumpbase`, `/ifcOutput`, …); `spelling`, as the command writes it (`-msmall-data-limit=` of what is `-G`); `index` and `count`, the command's arguments it takes; and `values`.

A source's language is named as `-x` names it (`c`, `c++`, `c++-module`, `objective-c`, `assembler`, `cuda`, …): the one `/Tc` or `/Tp` names it with, the one the last `-x` before it sets (`-x none` resets it), the one `/TC` or `/TP` sets, else its suffix's, `.C`, `.M` and `.S` naming other languages than in lower case but to cl, and `.c` and `.i` C++ to a C++ driver (`g++`, `c++`, `clang++`, `--driver-mode=g++`). An input after `-x`, or with `/TC` or `/TP`, is a source of its language whatever its suffix, but an object or a library.

### Edits

The edits of `edit` are made in turn. Each has `kind` and `flags`: arguments written as the command's program takes them.

| `kind`    | What it does                                                                                                                                                                                                                                                                                                                                                                                                          |
| --------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `remove`  | Takes out the arguments the flags name, and those an edit before it added: `-DX` the arguments `-DX` (`-DX=1` has another value), `-D*` (or `-D` last) every definition, `-W*` every option of `-W`'s group written with `-W` first. An option the table does not know is named as written, `=*` after it for any value, and one of those that take a separate value (`-dumpbase`, `/ifcOutput`) alone for any value. |
| `append`  | Adds the flags after the command's own arguments, where they take effect over them and libraries go: before `--` and `/link`, which end the options.                                                                                                                                                                                                                                                                  |
| `prepend` | Adds the flags right after the program.                                                                                                                                                                                                                                                                                                                                                                               |

The arguments no edit takes out keep the command's own spelling, and the added flags go as they are: catter writes no argument anew. A compiler's response file stays as the command names it unless an edit takes out an argument it holds or adds flags among them, which puts the arguments it holds in its place. Only a command whose arguments catter reads can be edited: a compiler's or a tool's, not `unparsed`.

### `analyze`

A request a handler sends catter, for what catter makes of commands of its own: `programs`, each with `executable`, `args` (the program's name first) and, optionally, `cwd`, the directory it runs in, which its relative response files are read from: without it, only its absolute ones are read. The result is `analyses`, one for each, `null` for a program catter knows nothing of.

### `error`, `exit` and `finish`

| Message  | Fields                                                                                                                                                                                           |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `error`  | `id`, `parent` (optional), `message`: why catter could not capture the command.                                                                                                                  |
| `exit`   | `id`, `code`: its exit code, 128 and the signal's number for one a signal ended, `duration`: how long the command ran, in microseconds, as the capture backend measures it (`0` under `replay`). |
| `finish` | `code`: the build's exit code, as `exit`'s. The result is `null`.                                                                                                                                |

## Strings and numbers

- **OS strings.** An argument, a path or an environment entry is a JSON string when its bytes are valid UTF-8, and `{"base64": "<the bytes in base64>"}` otherwise: on Linux and macOS they are bytes, which need not be UTF-8. A handler hands such a value back as it got it, or builds one the same way.
- Every other string is valid Unicode. JSON text has no byte order mark and no object repeats a key.
- An optional field that is absent may be left out or be `null`.
- Every integer of version 1 is below 2<sup>53</sup>, which a JavaScript number holds exactly.

## Errors

A handler that cannot answer a request answers with a JSON-RPC error; its `message` is what catter reports. catter ends the session when a handler answers with an error, with `abort` or with an action catter cannot take, stops (its output ends), writes what the protocol does not allow, or does not answer within `--handler-timeout`: the command being decided, and every command the build starts after it, fail without running, but under a backend that only observes the build, where every command has started by the time catter hears of it and the build runs on to its end; once the build has ended, catter ends the handlers' input without a `finish`, prints the reason and exits 1.

An observer cannot answer a notification: one that fails exits, which catter reports when it next writes to it.
