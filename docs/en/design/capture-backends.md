# Capture Backends

A capture backend is how catter gets a build's commands: it starts the build, and reports each command the build starts to catter, which has the session's handlers decide it and tells them how it ended. The handlers see the same commands, decisions and events whichever backend reports them.

## Choosing one

`--capture <backend>` names the backend that captures the build: `hook`, the default, or `wrapper`, on Linux and macOS. `--from <file>` replays a recorded build instead of running one: a recorded build is not captured, so `--from` takes no `--capture`, and a name no backend has is an error too. Both are reported before anything runs.

A handler learns the backend from the session's `initialize` message, whose `mode` is its name, or `replay` for a recorded build.

## What a backend does

- **It starts the build**, and catter ends as the build did. catter stands in for the build command: a SIGTERM, SIGINT or SIGHUP sent to catter goes on to it, and the build command ends when catter is killed.
- **It reports each command before it runs**: its working directory, program, arguments and environment, and the command that started it. The commands of a parallel build come at once. A command catter could not capture comes as an `error` instead.
- **It runs each command as the handlers decide**: as it is (`skip`), changed (`modify`, `edit`), or not at all (`drop`, which ends it with exit code 0). Once a handler aborts the build or fails, the command and every command the build starts after it end with exit code 1 without running.
- **It reports how each command ended**, before any command the build starts once that command has ended, and before the build's own end.

## Backends

### hook (default)

- **How it works**: a hook library is loaded into every process of the build and intercepts process creation (the `execve` / `posix_spawn` family on Linux and macOS, `CreateProcess` on Windows). On Linux and macOS each command then runs through `catter-proxy`, which asks catter and runs the command as decided, with the hook; on Windows the hook asks catter itself, and starts the program as catter decides. See [Hook mechanism](hook-mechanism.md) and [IPC protocol](ipc-protocol.md).
- **Platforms**: Linux, macOS, Windows.
- **What it sees**: every process of the build, each with the command that started it, but for what a program the hook cannot be loaded into starts: the program is reported and runs as decided, without the hook. On macOS those are Apple's own tools (`/bin/sh`, `/usr/bin/make`), which run as arm64e and from which System Integrity Protection drops the hook, and Intel programs under Rosetta; on Windows, 32-bit (x86) programs. A program a Windows program starts under another account is not seen at all.
- The build and its commands run on catter's own stdin, stdout and stderr: they write straight to the terminal, with their colors and in the order they write.

### replay (`--from`)

- **How it works**: it plays the build a file recorded instead of running one: each command, error and exit in the file's order, with the IDs and parent commands the file gives them. The build ends with the exit code the file records.
- **Actions**: nothing runs, so `skip`, `drop`, `modify` and `edit` change nothing. `abort` ends the replay as it ends a live build: no later event reaches the handlers, and catter exits 1.

### wrapper

- **How it works**: catter puts shims of the compilers, linkers and archivers it knows by their names, each the first of its name on the build's `PATH`, in a directory of the session's own, first on the build's `PATH`. A variable such as `CC`, `CXX` or `AR` that names a program by its path names that program's shim instead, and CMake runs every compile and link through the wrapper backend's launcher (`CMAKE_<LANG>_COMPILER_LAUNCHER` and `CMAKE_<LANG>_LINKER_LAUNCHER`, unless the build sets them). A shim is `catter-proxy`: it reports its command, runs the program it stands in for as decided, the first of its name on `PATH` past the shims, and reports how it ended. The program it runs finds no shims on its `PATH`: a compiler a build cache runs, as ccache runs the one past its own directory, is the compiler, not the shim again.
- **Platforms**: Linux, macOS.
- **What it sees**: the programs it has shims of, whatever in the build runs them, Apple's `/usr/bin/make` and `/bin/sh` included. A command's parent is the nearest program with a shim that started it, or the build command; what a compiler starts is not seen.
- **Limitations**: a build that runs a compiler by the path it found before catter ran, as the `build.ninja` CMake wrote does, escapes the shims: configure it and build it in one session, `catter compile-commands --capture wrapper -- sh -c "cmake -B build && cmake --build build"`. The shims go with the session, so a build configured under the wrapper backend runs only in that session: configure one for catter of its own. A program the build puts first on `PATH` itself, as a toolchain's directory, is found before the shims.

## Capability matrix

| Backend | Platforms               | How it captures                        | Actions                               | Parent command              | Status                |
| ------- | ----------------------- | -------------------------------------- | ------------------------------------- | --------------------------- | --------------------- |
| hook    | Linux / macOS / Windows | a hook in every process of the build   | `skip` `drop` `abort` `modify` `edit` | the process that started it | implemented (default) |
| replay  | any                     | a recorded build (`--from`)            | only `abort` takes effect             | as recorded                 | implemented           |
| wrapper | Linux / macOS           | compiler shims on `PATH`, `CC` / `CXX` | `skip` `drop` `abort` `modify` `edit` | the nearest wrapped program | implemented           |

## Writing handlers for any backend

- Do not assume one: `initialize` names it (`mode`).
- A command's parent is the command that started it as far as the backend sees: features that rely on the process tree (command trees, target trees) should not assume every process in between is there.
