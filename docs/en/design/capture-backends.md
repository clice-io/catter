# Capture Backends

A capture backend is how catter gets a build's commands: it starts the build, and reports each command the build starts to catter, which has the session's handlers decide it and tells them how it ended. The handlers see the same commands, decisions and events whichever backend reports them.

## Choosing one

`--capture <backend>` names the backend that captures the build: `hook`, the default, `wrapper`, `eslogger`, on macOS, or `ptrace`, on Linux. `--from <file>` replays a recorded build instead of running one: a recorded build is not captured, so `--from` takes no `--capture`, and a name no backend has is an error too. Both are reported before anything runs.

A handler learns the backend from the session's `initialize` message, whose `mode` is its name, or `replay` for a recorded build, and whose `decisions` says whether the build's commands wait for the handlers' decisions.

## What a backend does

- **It starts the build**, and catter ends as the build did. catter stands in for the build command: a SIGTERM, SIGINT or SIGHUP sent to catter goes on to it, and the build command ends when catter is killed, but under `eslogger`.
- **It reports each command**: its working directory, program, arguments and environment, and the command that started it. The commands of a parallel build come at once. A command catter could not capture comes as an `error` instead.
- **It follows the handlers' decisions, or only observes.** A backend that follows them reports each command before it runs, and runs it as the handlers decide: as it is (`skip`), changed (`modify`, `edit`), or not at all (`drop`, which ends it with exit code 0). A backend that only observes hears of each command once it runs, which no decision changes then: catter refuses a handler that decides (`decides` in its answer to `initialize`) before the build starts, and names it.
- **A handler that aborts the build or fails ends the session**: no handler is told anything after it. A command that has started runs on. Under a backend that follows decisions, the command being decided and every command the build starts after it end with exit code 1 without running; under one that only observes, every command has started by the time catter hears of it, and the build runs on to its end. catter then exits 1.
- **It reports how each command ended**, before any command the build starts once that command has ended, and before the build's own end.

## Backends

### hook (default)

- **How it works**: a hook library is loaded into every process of the build and intercepts process creation (the `execve` / `posix_spawn` family on Linux and macOS, `CreateProcess` on Windows). On Linux and macOS each command then runs through `catter-proxy`, which asks catter and runs the command as decided, with the hook; on Windows the hook asks catter itself, and starts the program as catter decides. See [Hook mechanism](hook-mechanism.md) and [IPC protocol](ipc-protocol.md).
- **Platforms**: Linux, macOS, Windows.
- **What it sees**: every process of the build, each with the command that started it, but for what a program the hook cannot be loaded into starts: the program is reported and runs as decided, without the hook. On macOS those are Apple's own tools (`/bin/sh`, `/usr/bin/make`), which run as arm64e and from which System Integrity Protection drops the hook, and Intel programs under Rosetta; on Windows, 32-bit (x86) programs. A program a Windows program starts under another account is not seen at all. On Linux, a program that a static program, or Go, starts with an exec of its own, which the hook does not see, reports itself once the hook loads into it, as a child of the nearest command catter captured, and runs as decided: it starts again through `catter-proxy`, or ends with exit code 0 when dropped. A static program that replaces itself with another program whose arguments end with its own, as one started with no arguments does with any, is reported as itself, and a program that valgrind runs inside its own process runs there, unreported.
- The build and its commands run on catter's own stdin, stdout and stderr: they write straight to the terminal, with their colors and in the order they write.

### eslogger (macOS)

- **How it works**: catter runs the build as it is, and Endpoint Security, the macOS interface for observing processes, reports every process the build starts through `eslogger`, the program macOS has for it (`eslogger exec fork exit`). catter follows the build's processes from the build command's: each program a process of the build runs is a command, whose parent is the program its process ran before it, or the command whose process forked its own. catter reads a command once it runs, and the build never waits for catter.
- **Platforms**: macOS.
- **What it needs**: root, through `sudo`, for eslogger: sudo asks for the password on the terminal before the build starts, and catter refuses to run where it cannot, unless sudo runs `/usr/bin/eslogger` without a password. Endpoint Security takes no program without Full Disk Access for the app catter runs in: Terminal, or another terminal app, in System Settings > Privacy & Security > Full Disk Access; over SSH, "Allow full disk access for remote users" in the settings of Remote Login.
- **What it sees**: every program the build's processes run, Apple's own tools (`/bin/sh`, `/usr/bin/make`, `xcodebuild`) and what they start included. A program that `/usr/bin/cc` or `/bin/sh` runs in its place (Xcode's `clang`, `bash`) is a command of its own, a child of the one it replaced. A process the build does not start itself, as a service started by `launchd` on the build's behalf or a server started before the build, is not seen, and neither is a program that fails to start.
- **Actions**: none; a handler that decides is refused.
- **Costs**: catter waits for eslogger to report before it starts the build, and for its last events once the build has ended: about a second in all. eslogger may fall behind a build that starts many processes at once: catter takes every event, which can take seconds after the build has ended. Endpoint Security may drop events eslogger falls far behind on: catter warns how many of the system's it dropped while the build ran, and commands of the build may be missing, with every command a lost process started.
- **Limitations**:
  - macOS ends no program with the one that started it: a build catter stands in for ends on SIGTERM, SIGINT and SIGHUP, which catter passes on, but not when catter is killed.
  - What catter reads of a command's files, such as its response files (`@file`), it reads once the command has run, or is running: a file the build removes once the command has run, as Ninja does a response file, may be gone.

### ptrace (Linux)

- **How it works**: catter runs the build traced, as a debugger follows a program: the kernel stops each process of the build as it starts a process or a thread, runs a program and ends, and catter reads the program it runs, its arguments, environment and working directory from `/proc` before the process goes on. catter follows the build's processes from the build command's, as under `eslogger`: each program a process of the build runs is a command, whose parent is the program its process ran before it, or the command whose process forked its own. A program runs once the handlers have been told of its command, whose files they read as they are then, but which no decision changes: catter never changes what a process runs.
- **Platforms**: Linux.
- **What it needs**: a system that lets a program trace its own children: `kernel.yama.ptrace_scope` 0 or 1, as most distributions set it, and in a container a profile that allows `ptrace`, as Docker's default does on Linux 4.8 and later. Where the system does not, catter says why before the build starts.
- **What it sees**: every program the build's processes run, a static program's and one a program starts without the C library included, as Go programs start theirs. A script is a command as the system runs it: its arguments are the interpreter its `#!` line names, the line's argument if it has one, then the script and the script's arguments. A process the build does not start itself, as a server started before the build, is not seen, and neither is a program that fails to start.
- **Actions**: none; a handler that decides is refused.
- **Costs**: no program starts in a command's place, and catter is told of no system call: the kernel stops a process of the build as it starts a process or a thread, runs a program and ends, and as a signal reaches it, while catter reads what it is told; a program waits at its start until the handlers have been told of its command. A make build of 1500 compiles took about an eighth longer than without catter, where the hook backend took a third longer.
- **Limitations**:
  - Unless catter runs as root, a traced program runs without the privileges its file grants (set-user-ID, set-group-ID or capabilities), as it would under any debugger: `sudo` fails. catter warns of each such program; a build that needs one runs under `hook`.
  - A process has one tracer: a program of the build that traces another program of the build, as gdb, strace or a crash reporter does, fails to. So does LeakSanitizer, which a program built with AddressSanitizer runs at its exit: it fails, saying that LeakSanitizer does not work under ptrace, unless `ASAN_OPTIONS=detect_leaks=0` turns it off.
  - catter ends with the build command: a process the build leaves running then, as a build server, runs on untraced, and what it starts later is not seen.

### replay (`--from`)

- **How it works**: it plays the build a file recorded instead of running one: each command, error and exit in the file's order, with the IDs and parent commands the file gives them. The build ends with the exit code the file records.
- **Actions**: nothing runs, so `skip`, `drop`, `modify` and `edit` change nothing. `abort` ends the replay as it ends a live build: no later event reaches the handlers, and catter exits 1.

### wrapper

- **How it works**: before each directory on the build's `PATH`, catter puts one of the session's own, with a shim of each compiler, linker and archiver it knows by its name there: whichever names a build tool looks for, and in whichever order, it finds the program it would without catter, through its shim. A variable such as `CC`, `CXX` or `AR` that names a program by its path names that program's shim instead, and CMake runs every compile and link through the wrapper backend's launcher (`CMAKE_<LANG>_COMPILER_LAUNCHER` and `CMAKE_<LANG>_LINKER_LAUNCHER`); on Windows MSBuild runs `cl.exe`, `link.exe` and `lib.exe` from the shims (`CLToolPath`, `LinkToolPath` and `LibToolPath`), each unless the build sets it. A shim is `catter-proxy`: it reports its command, runs the program it stands in for as decided, and reports how it ended. The program it runs finds no shims on its `PATH`: a compiler a build cache runs, as ccache runs the one past its own directory, is the compiler, not the shim again.
- **Platforms**: Linux, macOS, Windows.
- **What it sees**: the programs it has shims of, whatever in the build runs them, Apple's `/usr/bin/make` and `/bin/sh` included. A command's parent is the nearest program with a shim that started it, or the build command; what a compiler starts is not seen.
- **Limitations**: a build that runs a compiler by the path it found before catter ran, as the `build.ninja` CMake wrote does, escapes the shims: configure it and build it in one session, `catter compile-commands --capture wrapper -- sh -c "cmake -B build && cmake --build build"`. The shims go with the session, so a build configured under the wrapper backend runs only in that session: configure one for catter of its own. A program the build puts first on `PATH` itself, as a toolchain's directory, is found before the shims, and one found through a relative entry of `PATH`, which names another directory wherever the build goes, has no shim. On Windows only `.exe` programs have shims, and a project that sets `CLToolPath`, `LinkToolPath` or `LibToolPath` itself, as a toolset other than MSVC's may, runs those tools past them.

## Capability matrix

| Backend  | Platforms               | How it captures                        | Actions                               | Parent command              | Status                |
| -------- | ----------------------- | -------------------------------------- | ------------------------------------- | --------------------------- | --------------------- |
| hook     | Linux / macOS / Windows | a hook in every process of the build   | `skip` `drop` `abort` `modify` `edit` | the process that started it | implemented (default) |
| eslogger | macOS                   | Endpoint Security, through `eslogger`  | none: it only observes                | the process that started it | implemented           |
| ptrace   | Linux                   | ptrace, as a debugger follows programs | none: it only observes                | the process that started it | implemented           |
| replay   | any                     | a recorded build (`--from`)            | only `abort` takes effect             | as recorded                 | implemented           |
| wrapper  | Linux / macOS / Windows | compiler shims on `PATH`, `CC` / `CXX` | `skip` `drop` `abort` `modify` `edit` | the nearest wrapped program | implemented           |

## Writing handlers for any backend

- Do not assume one: `initialize` names it (`mode`), and says whether the commands wait for decisions (`decisions`). A handler that can do without deciding answers `decides` as `decisions` says.
- A command's parent is the command that started it as far as the backend sees: features that rely on the process tree (command trees, target trees) should not assume every process in between is there.
