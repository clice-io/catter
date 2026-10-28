# Build Profiling

`profile` records when each command of the build started, how long it ran and which command started it, and writes them as a trace that [Perfetto](https://ui.perfetto.dev) and `chrome://tracing` open: the build's timeline, with how many of its commands ran at once, the chain of steps that set its length, and the steps that took the longest.

Command: `profile`

## Usage

```bash
catter profile [options] -- <build-command>
```

## Options

| Option                | Description                                                                                     |
| --------------------- | ----------------------------------------------------------------------------------------------- |
| `-o, --output <path>` | Write the trace to this path; a relative one is from the `-C` directory. Default: `trace.json`. |
| `--top <n>`           | List the n commands that took the longest. Default: 5; `0` lists none.                          |
| `-q, --quiet`         | Print no summary, nor why catter could not capture a command.                                   |

`profile` also takes the options of every command that runs a build: `-C`, `--capture`, `--from` and the handlers' (see [Configuration](../guide/configuration.md)).

catter writes the trace once the build has ended, whether it succeeded or failed, making the directory it goes in when there is none, and exits as the build did. When it cannot write the trace, it says why and exits with 1; a build a handler aborts gets no trace. Unless `-q` is given, catter prints why it could not capture a command as it hears of it.

## The trace

The trace is a JSON file in Chrome's trace event format. Open it in [ui.perfetto.dev](https://ui.perfetto.dev) (Open trace file), or in `chrome://tracing` (Load). Its times are in microseconds from when the first command started.

- **Lanes.** Each command is a slice on a lane. A command runs within the command that started it, on its lane, when it starts and ends while that command runs and the commands that command started never ran two at once: a shell and the compiler it runs, a compiler driver and its stages (`cc1plus`, `as`, `collect2`, `ld`). The commands a build tool runs at once each go on the first lane where nothing runs as they start: under `make -j8`, lane 1 holds `make`, and the lanes below it its jobs, as many as ran at once.
- **critical path.** A track above the lanes holds the commands of the critical path (see below).
- **running.** A counter of how many commands work at each moment. A command works while it runs and no command it started does; a compiler or archiver command catter analyzes works for as long as it runs, together with the programs it starts.

Each slice is named by its program and, for a command catter analyzes, the file it is about: the source a compile compiles (`c++ main.cc`), the file a link or an archive makes (`c++ app`, `ar libfoo.a`). Its category is what the command does, for one catter analyzes: `compile`, `link`, `preprocess`, `archive`, ... Its arguments hold:

| Argument  | Holds                                                                                                                        |
| --------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `command` | Its arguments, on one line: each as it is when it holds only ASCII letters, digits and `_@%+=:,./\-`, else as a JSON string. |
| `cwd`     | The directory it ran in.                                                                                                     |
| `exit`    | Its exit code; 128 and the signal's number for one a signal ended.                                                           |
| `id`      | Its id in the session, which `parent` names.                                                                                 |
| `parent`  | The id of the command that started it; none for the build command.                                                           |
| `writes`  | For a command catter analyzes, the files it writes.                                                                          |
| `target`  | For a compile, the files the commands that read its object write: the library or the program the object goes into.           |

## The summary

Unless `-q` is given, catter prints what the trace shows once it has written it:

```
Profiled 1234 commands in 83.4 s: 6.2 ran at once on average, 16 at most.
Critical path, 48.9 s: c++ big.cc 41.0 s > ar libcore.a 400 ms > c++ app 7.5 s
Slowest:
  41.0 s  c++ big.cc
   7.5 s  c++ app
   3.1 s  python3
   2.9 s  c++ parser.cc
   2.0 s  cmake
Trace saved to /src/project/trace.json: open it in https://ui.perfetto.dev or chrome://tracing.
```

- **The first line** says how long the build took, from the first command's start to the last one's end; how many commands worked at once on average, the time they worked, added up, over how long the build took; and the most that worked at once.
- **The critical path** is the chain of compiles, archives and links, each reading a file the one before it wrote, whose steps took the longest, added up. The files it follows are those a compiler's or an archiver's arguments name: a program catter does not analyze, as a code generator, and the headers a compile includes are not in it, so the chain can be shorter than the one that set the build's length. A path of more than six steps shows its first three and last two.
- **The slowest** commands are those that worked the longest. A build tool, a shell or a script is counted only for the time it worked itself: `make` reading its makefiles, `cmake` configuring, but not the compiles they wait for.
- When commands are left out of the trace, a line says how many: those catter could not capture, and those that did not end before the build did, as a build server that outlives it.

## Times and capture backends

Every capture backend gives each command's start and duration:

| Backend    | Start and duration                                                                                                                                                                                                                                                                                                                                                                                                             |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `hook`     | On Linux and macOS, `catter-proxy` measures each command, from when it starts the program catter told it to run to the program's exit; on Windows, catter does, from when it takes over the program the hook started, suspended, to its exit. Each program of the build waits for catter's decision before it starts, and on Linux and macOS starts through `catter-proxy`: the trace shows the build as it runs under catter. |
| `wrapper`  | As under `hook`, for the compilers, linkers and archivers the shims stand for, and the build command: the trace holds only those.                                                                                                                                                                                                                                                                                              |
| `ptrace`   | The times catter's tracer sees each process exec its program and exit. Each process of the build stops at each fork, exec and exit while catter reads it.                                                                                                                                                                                                                                                                      |
| `eslogger` | The times Endpoint Security gives each exec and exit. It only observes: the build runs as it would without catter.                                                                                                                                                                                                                                                                                                             |
| `--from`   | The times the file records, in microseconds: an event's `execution` holds its `start`, since the session started, and its `duration`. A time it does not record is `0`.                                                                                                                                                                                                                                                        |

## Use Cases

- **Identify serialization bottlenecks** -- Find stages where the build runs one command at a time despite available cores: the `running` counter drops to one.
- **Measure actual parallelism** -- Compare how many commands ran at once with the number of cores, and with the `-j` the build tool was given.
- **Find slow compilation units** -- Pinpoint the sources that take disproportionately long to compile, and those on the critical path.
- **Debug build system configuration** -- Detect misconfigured dependencies, unnecessary serialization, or suboptimal job scheduling.
