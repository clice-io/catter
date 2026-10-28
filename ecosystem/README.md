# ecosystem

catter over real projects, pinned and unmodified, each built the ways its
users build it: the matrix of project × build system × backend × platform.
`tests/` holds catter's own small projects, one mechanism each, and runs on
every pull request; the ecosystem runs nightly and only reports. A
difference it finds that catter fixes gets a case of its own in `tests/`.

The reference is never the build's own compilation database, which can be
wrong too. A round checks what catter answers against what it cannot
influence: a recorder of every exec around the same run, the files the
build wrote, and the compiler itself.

This file is the contract: what a project holds, what a job does and checks,
what makes it red, and how CI runs a round. The framework is
`tools/ecosystem/`.

## Projects

```
ecosystem/
  <project>/
    project.toml     where the project comes from, how it builds, its known differences
    README.md        why it is here: what it covers that no other project does
    <tree>/          optional: a small program of our own using the project, for a build
                     that takes it as a package (WrapDB, the Bazel Central Registry)
```

One directory per project, named as the project names itself; a project
built several ways is one directory with several builds.

### project.toml

```toml
about = "fmt 12.0.0 with its tests"
repository = "https://github.com/fmtlib/fmt"
commit = "e424e3f2e607da02742f73db84873b8084fc714c"

[builds.cmake-ninja]
about = "CMake and Ninja, its tests on, CMake's database exported"
backends = ["hook", "wrapper"]
configure = [["cmake", "-S", ".", "-B", "build", "-G", "Ninja", "-DFMT_TEST=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]]
build = [["cmake", "--build", "build", "-j", "${JOBS}"]]
hint = "build/compile_commands.json"

[[known]]
issue = "…"
why = "…"
build = "cmake-ninja"
backend = "wrapper"
check = "recorder"
kind = "missed"
detail = "…"
```

- **Source.** `repository` and `commit` (a full hash) fetch that commit
  alone; an `[archive]` table (`url`, `sha256`) downloads and checks one.
- **Builds**, each `[builds.<name>]`, named by its build system
  (`cmake-ninja`, `meson`, `cargo`, `pip`):
  - `about`, for a reader;
  - `platforms`, of which `linux` alone runs today (`["linux"]` when
    absent);
  - `backends` of `hook` and `wrapper` (`["hook"]` when absent);
  - `packages`, the Debian packages it needs on Linux;
  - `tree`, a directory of the project's own built in place of its source;
  - `env`, variables of its build;
  - `prepare`, steps outside the session before anything else: what the
    project needs that no build of it does (a toolchain, a virtual
    environment);
  - `configure`, steps outside the session, but under the `wrapper`
    backend, whose shims must be on PATH when a build system resolves its
    compilers;
  - `build`, the steps catter runs as the build;
  - `scan`, the tree-relative directories whose new files are the build's
    artifacts (`["."]` when absent; one that is a symlink, as Bazel's
    `bazel-out`, is followed);
  - `hint`, the tree-relative compilation database the build system writes.

  A step is an argv. `${JOBS}` reads the machine's core count, `${TREE}`
  the build's tree, `${WORK}` a scratch directory beside it (Bazel's output
  root).

- **Jobs.** A job is a build on one platform, `<project>/<build>`, which
  runs each of its backends in turn; one runner each.

## A job

For each backend, on a fresh copy of the source:

1. `prepare`, then `configure` (unless the backend is `wrapper`).
2. The recorder starts; catter runs the build, `configure` first under
   `wrapper`:
   `catter compile-commands --capture <backend> -o … --handler "node tools/ecosystem/handler.ts …" -- <build>`.
   The handler only observes: it writes each command catter reports, with
   its environment and catter's analysis, each capture error and exit, and
   the build's end. The recorder stops.
3. `catter targets --from` reads the replay of those events.
4. The checks.

A step outside the session that fails, or a build that fails under catter
and, run again without catter, fails there too, leaves the backend not
checked: the setup's failure, never catter's.

## The checks

Each finds **differences**, of a check and a kind, with a one-line detail in
which the places of the run read `<tree>`, `<work>`, `<home>` (catter's data
directory) and `<catter>` (its directory).

- **recorder**: the execs the recorder saw against catter's commands.
  catter's own processes (catter, catter-proxy, the wrapper's shims) and the
  handler's are dropped from the recording, and each exec's parent is its
  nearest ancestor left, which is catter's model of a command's parent;
  execs compare by argv, not by executable, which the recorder resolves
  through links. Under `hook` the two trees match one to one (`missed`,
  `extra`, `cwd`). Under `wrapper` catter sees only the programs its shims
  stand in for: each of its commands is among the recorder's (`extra`),
  under the nearest one it reported too (`parent`), and every compiler,
  linker and archiver the build itself started, by name, is among its
  commands (`missed`); one a compiler driver started is the driver's. Then
  the compiles the recorder saw (a compiler, by name, naming a source, and
  not only preprocessing it: `-E`, `-M`, `-MM`) against the database: each source still there has an entry (`no-entry`),
  and each entry is a recorded compile's (`entry-without-compile`). A recording that
  lost events says so (`lost`).
- **artifacts**: the files the build wrote under `scan`, by their magic:
  each object (ELF relocatable, LLVM bitcode) is the
  output of exactly one entry, or of a relocatable link
  (`object-without-entry`, `object-in-several-entries`); each archive,
  shared library and executable is written by a link or archive command of
  catter's analysis, the commands `catter targets` is made of, or is a hard
  link of one (`archive-without-command`, `linked-without-command`).
- **syntax**: each entry compiled again with `-fsyntax-only` instead of its
  object and dependency files, in its directory and with the environment
  of the command it came from, succeeds (`fails`). An entry whose source the
  build removed, or of a compiler that does not take GCC's options, is
  counted, not checked.
- **hint**: catter's database against the build system's, by directory,
  source and output (`catter-only`, `build-only`, `arguments`, `no-hint`).
- **catter**: catter exits as the build does (`exit`), its handler is told
  the build ended (`no-finish`), it captures every command
  (`capture-error`), its logs hold no error (`log-error`), and
  `catter targets --from` reads the replay (`targets`).

### Known differences

A `[[known]]` table of the project names a difference already understood:
its `check`, `kind` and a regular expression its `detail` matches, narrowed
by `build` and `backend` when given, with `why` for the reader
and, when catter is wrong, the `issue` it is recorded as. A difference
where the build system's database is the wrong one, or that the build
itself makes (a Makefile renaming the objects it compiles), has a `why`
alone. One is as narrow as its difference: a pattern matching a whole kind
would hide the next difference of that kind. A known difference catter
fixes goes once a nightly holds the fix: that nightly's round lists it
among the known differences no run showed.

## Recorders

- **Linux: auditd.** A rule logs every `execve`, `execveat` and fork of the
  machine under the run's key; the images come from the raw log, its
  `SYSCALL`, `EXECVE` and `CWD` records grouped by serial. A process's
  parent is the image its process ran when it forked it, through a daemon's
  double fork (Bazel's server). It needs auditd running, sudo, and a log
  that does not rotate during the build (`max_log_file_action = ignore`, as
  the workflow sets it; a log that rotated stops the job); it costs the
  build about a tenth.
- **Linux, without auditd: strace** (`--recorder strace`), each process's
  directory rebuilt from its `chdir`s and forks. It slows a build 1.75 times
  and breaks a sanitized catter (LeakSanitizer does not run under ptrace).

## Reports

A job writes `report.md` and `results.json`: per backend its numbers (execs
recorded, catter's commands, entries, artifacts, entries checked), then its
new differences, each with what reproduces it (the run, and for a failing
entry the compile), then the known ones with their issue or reason, then the
known ones no backend showed (fixed, or hidden by another). Beside them, for
each backend: catter's database, the handler's events and their replay, the
recording, the build's output and catter's logs.

A job is red when it stops before its checks (a source it cannot fetch, a
recorder that does not record), when a backend is not checked, or when one
finds a new difference; known differences alone leave it green. The round's
summary lists every job, one that left no results red. **A round shows no new difference** when
every job is green: what a stable release requires of the nightly it tags.

## CI

`.github/workflows/ecosystem.yml` runs a round: every job, each on a Linux
runner, all at once, then a job gathering their summaries.
`nightly.yml` dispatches it with the nightly it has just published, whose
release package is the catter under test. A dispatch by hand names a nightly's
tag, or a CI run (`main.yml`) whose package artifacts live (30 days); a pull
request that changes `ecosystem/` or `tools/ecosystem/` runs a round on the
newest nightly, which checks the change itself: a new project's
differences, a known one's pattern. The framework is the workflow's own
ref. The round only reports: nothing waits on it but a stable release.

## Running

```
node tools/ecosystem/run.ts --list                                     every job of a round, as JSON
node tools/ecosystem/run.ts <project>/<build> --dir <dir> [options]    a job
node tools/ecosystem/run.ts round <dir>                                the summary of the results under <dir>
```

`CATTER_EXECUTABLE` names the catter under test. `--dir` is where projects
are fetched and built, `--out` where the results go (`<dir>/out/<job>` when
absent), `--backend` narrows the job to some of its backends, `--recorder`
picks `auditd` (Linux's default), `strace` or `none`. A job runs in the
pixi `ecosystem` environment, which brings the build tools (CMake, Ninja,
Meson, Make) but no compiler: the projects build with the system's, as a
user's do. Building a project takes cores and minutes: a box, never this
machine.

## Framework

```
tools/ecosystem/
  project.ts      project.toml: builds, known differences, jobs
  source.ts       a project's pinned source, and each build's copy of it
  job.ts          a job: each backend's run and checks
  handler.ts      the handler catter runs with, which writes the session's events
  session.ts      those events, and their replay
  steps.ts        a build of several steps, run as one
  record/         the recorders: auditd, strace, the images of what they saw
  checks/         recorder, artifacts, syntax, hint
  command.ts      what the checks know of a command line, without catter
  difference.ts   differences, known ones, the places a detail names
  report.ts       a job's summary and a round's
  run.ts          the command line
  **/*.test.ts    the framework's own tests, beside the code
```
