# tests

Every test of catter's behavior is a case here: catter, built by any build
system, run on a small project, and every output it gives pinned in a
snapshot. The cases are what tells that a change of catter's internals, its
build or its language left its behavior as it was. They follow clice's test
system (clice's `tests/README.md`); this file is the contract for catter's.

## Cases

```
tests/<topic>/<subtopic>/NN_name/
  inputs…      any files: sources, build files, scripts, replay files
  test.ts      optional: what to run
  README.md    optional: notes for a reader of the case
  snap.toml    every output the case pins
```

- **Two levels, always.** A topic is a product area, a subtopic one aspect of
  it. Case directories are numbered (`01_name`, two digits, lowercase, `_`
  between words); the number orders the cases, gaps are fine.
- **Self-contained.** A case reads nothing outside its directory but the
  `test.ts` of its subtopic and topic. Nothing is shared between cases; a
  project or a helper program two cases need is in both.
- Maintainer notes go to `//` comments in `test.ts` or at the top of the
  inputs.

### Topics

- **Engine**, what catter does to a build as it runs it: `capture` (how
  programs started during a build reach catter and run: arguments,
  environment, working directory, stdio, exit status, process trees, the
  functions that start programs), `builds` (real build systems: make, CMake
  with Ninja, MSBuild, xmake), `scripts` (user scripts: their actions,
  errors, modules, lifecycle), `handlers` (handlers over the protocol, on
  the SDKs: their actions, errors, lifecycle, transports, several at once,
  arguments that are not UTF-8, the analysis of commands, the TypeScript
  SDK).
- **Builtins**, what a builtin script answers on the commands it saw: `cdb`
  (`script::cdb`), `cmd_tree`, `target_tree`, and `inspect`
  (`script::inspect`, how catter analyzes one command, which it does not
  run: the compiler, its mode and target, the files it reads and writes).
- **Command**, catter's own command line: `cli` (options, modes, sessions).

`tools/testing/cases/topics.ts` lists them; any other directory here is an
error. C++ unit tests are not cases: they live in `unittests/` at the
repository root.

## Inputs

- **Every test runs on a fresh copy** of the case's inputs, and with a data
  directory of its own (`CATTER_HOME`) beside it: its logs and its session
  pipe are its own. Nothing a test does reaches the repository or another
  test.
- **The copy's root is named past ASCII** (`catter-case-é中😀-…`, characters
  of two, three and four UTF-8 bytes), as a home or project directory often
  is, and so is the data directory: every case runs under such paths on every
  platform.
- **A file named `*.in`** is copied without the suffix, `${WS}` replaced by
  the copy's root: a database or response file that needs absolute paths.
- **Generated inputs** (a hundred sources) come from
  `t.disk.write(generate.sources(100))`, never from loops in a test.
- **The catter and the SDKs under test** reach every command a test runs:
  `CATTER_EXECUTABLE` names the catter, for a program that starts it
  itself; `import catter` finds the Python SDK; and a copy whose
  `package.json` depends on `@catter/sdk` has the TypeScript SDK in its
  `node_modules`.
- **A program a build runs** to show what reaches it is an input of the case:
  a `build.mjs` that starts programs the way a build tool does and prints
  what they received, or a C source the test compiles with `t.setup`. It
  prints what it observed, never a verdict: the snapshot is the verdict.

## test.ts

```ts
import { test } from "@catter/tools/testing";

test(async (t) => {
  await t.setup("cmake -S . -B build -G Ninja");
  await t.cli("script::cdb -o compile_commands.json -- cmake --build build -j 1");
  t.show("compile_commands.json", t.disk.cdb());
});
```

A script registers tests, each `test(fn)`, `test(name, fn)`,
`test(options, fn)` or `test(name, options, fn)`. Names are short and unique
in the script; one unnamed test is allowed when it is the only one. A case
without `test.ts` takes its subtopic's, then its topic's; a script sits at
the lowest directory every case running it shares, and one no case runs is
an error.

- `t.cli(command)` runs catter in the copy; its transcript joins the output.
- `t.run(command)` runs another program, as a build or a user does; its
  transcript joins the output too.
- `t.setup(command)` prepares the copy (a build's configure step): it must
  succeed, and what it printed shows only when it fails.
- `t.show(label, value)` adds a block: text, lines or a number under
  `value`, a table as it is.
- `t.disk` reads and changes the copy: `read`, `write`, `edit`, `rm`,
  `list`, `exists`, and `cdb(file)`, a compilation database as it shows.

A command is a string split as a POSIX shell splits plain words and quotes,
or a list of words; no shell runs it. Each takes `{ env, cwd, input }`.
Commands started side by side (`Promise.all`) keep in the output the order
they were started in.

**Test options**:

| Option                   | Means                                                                                            |
| ------------------------ | ------------------------------------------------------------------------------------------------ |
| `except: { win32: "…" }` | platforms the test has no meaning on, each with the reason; there it is listed as not applicable |
| `errors: ["…"]`          | the error-level log records the test provokes, as regular expressions; any other fails           |

## Output

`snap.toml` is the case's output: per test, every block in run order, the
table `[<test>.<label>]`. A test is named by its script, an unnamed one
`test`.

```toml
[test."$ catter script::cdb -o compile_commands.json -- make"]
stdout = '''
g++ -DCATTER_TEST=1 -Iinclude -c src/main.cc -o main.o
CDB saved to ${WS}/compile_commands.json with 1 entries.
'''
exit = 0

[test."$ catter script::cdb -o compile_commands.json -- make".darwin]
wrong = "dyld kills Apple's arm64e tools when the hook is inserted (catter#115)"
stdout = "CDB saved to ${WS}/compile_commands.json with 0 entries."
stderr = '''
dyld[#]: terminating because inserted dylib …
'''
exit = 0
```

- **Blocks.** A command's block is under its command line, `$ catter …`:
  `stdout`, `stderr`, and `exit` (or `killed` and the signal). A command run
  again in one test is `#2`. Other blocks are what the test showed. The file
  is TOML 1.1: a list or table stays on a line of 120 columns, else takes a
  line per item; text of several lines is a raw `'''` string.
- **Placeholders.** What differs from host to host reads the same
  everywhere: the copy `${WS}`, catter's data directory `${CATTER_HOME}`,
  catter's own directory `${CATTER}`, the pixi environment `${ENV}`, the
  temporary directory `${TMP}`, MSVC `${MSVC}`, the Windows SDK `${WINSDK}`,
  Xcode `${XCODE}`, each with `/` after it; the macOS SDK's version
  (`MacOSX#.sdk`), durations (`#ms`, `#s`), process ids (`dyld[#]`), native
  stack frames (`#… native frames`), xmake's probe files
  (`${TMP}/.xmake#/#/_#`). Line ends read LF and colors are dropped.
- **Platforms.** One file serves linux, darwin and win32. A block holds the
  output of the test's reference platform, the first of those three it runs
  on. Another platform whose output differs holds its own under the block,
  `[<test>.<label>.<platform>]`, with the reason: `wrong = "why"` for a bug,
  `differs = "why"` for what rightly differs there (another compiler,
  another shell). A platform block without a reason fails. A test shows the
  same blocks on every platform it runs on: one whose commands differ is a
  test per platform, each excepted where it has no meaning.
- **Known wrong.** A block whose current output is wrong carries the reason,
  `wrong = "a build argument with spaces is split on Windows (catter#151)"`, on the block or on
  its platform's. Every update keeps the reasons while the block exists.
- **Banned**: writing catter's output in a test, or matching it partially.
  Either the whole output is in the snapshot, or a test checks a state
  exactly.
- **Updating.** `UPDATE_SNAPSHOTS=1` rewrites the outputs of the platform it
  runs on and prints, per case, the blocks whose output changed — those
  known wrong (likely fixed) apart from the rest (likely regressed) — the
  blocks added and removed, and the platform blocks that still need a
  reason. A pull request carries that summary. Run it on the reference
  platform first: another platform compares with its blocks.

## Waiting and teardown

- **Waiting** happens only on events: a command's exit, a program's output.
  No sleeps, no polling, no wall-clock assertions. The lint rule
  `catter/timers` (`tools/lint`) holds every timer in `tools/` and `tests/`
  to this.
- **At teardown**, every test: catter wrote its log, which holds no
  error-level record the test did not declare. On failure a test prints its steps and catter's
  warnings and errors.
- **Time.** A command still running after 4 minutes is hung, and killed with
  every process it started; a script still running after 250 s fails with
  its steps.

## Unit tests

`unittests/` at the repository root holds the C++ tests (zest) of what no
product boundary observes cheaply, built with the sources.

- **No input files.** A unit test's inputs are in its code, or a layout it
  writes to a temporary directory; it reads nothing checked in.
- What a feature answers is a case, not a unit test. A check whose only
  result is pass or fail (an invariant, a parser's edge) stays a unit test.

## Budgets

- Pull-request CI under 20 minutes wall clock; no job over 10 minutes.
- A test under 30 seconds in Debug on four cores.

## Never

- Write catter's output by hand, or match it partially.
- Wait on, or assert about, wall-clock time.
- Retry, skip or weaken a failing test; `except` is for a test with no
  meaning on a platform, never for one that fails there.
- Share inputs between cases.
- Hand-edit a snapshot, but for the reasons on its blocks.

## Running

```
pixi run -e dev cases-test [mode] [i/n]   every case, or shard i of n, against build/*/*/<mode>
pnpm --dir tests run cases                every case, against $CATTER_EXECUTABLE
CATTER_CASES=capture/ …                   the cases under a path
CATTER_CASES_SHARD=2/4 pnpm --dir tests run cases   shard 2 of 4: whole subtopics, balanced by case count
UPDATE_SNAPSHOTS=1 …                      rewrite this platform's outputs, print the summary
CATTER_KEEP_WORKSPACE=1 …                 keep each test's copy and data directory, print where
pnpm --dir tools test                     the framework's own tests
```

Commands run in the pixi `dev` environment, which brings node, CMake and
Ninja; make, xmake and, on Windows, Visual Studio come from the machine.

## Framework

```
tools/testing/
  cases/       discovery, copies, scripts, the disk, snap.toml and its platforms
  cli/         command runs, their environment and transcripts, normalization
  render/      snap.toml's values and their layout
  logs/        catter's logs and the error gate
  runner/      the vitest entry points over tests/
  **/*.test.ts the framework's own tests, beside the code
```
