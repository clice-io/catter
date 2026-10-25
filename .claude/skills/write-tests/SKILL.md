---
name: write-tests
description: Read before writing or modifying anything under tests/ or unittests/, or the framework under tools/testing — the case workflow, unit tests, hard rules and known pitfalls.
---

# Writing catter tests

Every test but the C++ unit tests is a **case**: a directory
`tests/<topic>/<subtopic>/NN_name/` that is a small project of its own,
with the `test.ts` that says what to run (its own or inherited from its
subtopic or topic) and `snap.toml`, every output it pins. **`tests/README.md`
is the contract**: topics, inputs, the `test.ts` API (`t.cli`, `t.run`,
`t.setup`, `t.show`, `t.disk`), outputs, platforms, waiting and teardown.
Read it before writing a case; this skill adds only the workflow, the hard
rules and the pitfalls. The framework is `tools/testing/`
(`@catter/tools/testing`).

## Workflow

1. Place the case by product area, not by the way it checks: the topic and
   subtopic of the behavior (`tools/testing/cases/topics.ts` lists the
   topics). Copy the shape of 2-3 neighboring cases.
2. Write the inputs: a project a real build tool builds, or a `build.mjs`
   that starts programs the way a build tool does and prints what they
   received. A `test.ts` only when the inherited one does not fit; what a
   compiler or argument parser makes of one command is an `inspect` case,
   a line of its `commands.txt`.
3. Run it alone on the box the build ran on (the test skill):
   `CATTER_CASES=<topic>/<subtopic>/NN_name UPDATE_SNAPSHOTS=1 pixi run -e dev cases-test <mode>`,
   the mode of the build there (PowerShell on Windows: the test skill).
   The update prints what changed: review every block like code. Update on
   Linux first, the reference; then macOS and Windows, which write only the
   sub-blocks where they differ. Each sub-block, and each block of an answer
   catter gets wrong, carries its reason by hand: `wrong = "... (catter#N)"`
   or `differs = "..."`; updates keep reasons.
4. The snapshots are written on the box: bring them back before the next
   `box push` overwrites them, `snap.toml` files only (pitfalls).
5. Gate: `pixi run -e dev check-testing` (strict tsc, type-checked ESLint
   with `catter/timers`, the framework's tests), and the case on every
   platform it runs on.

## Hard rules

- A weakened assertion, a dropped block or an `except` for a test that
  fails somewhere is skipping a test (CLAUDE.md). `except: { <platform>: "why" }`
  is for a test with no meaning on a platform. A bug stays in the output,
  with `wrong` on its block.
- No timers, sleeps or polls: a wait is on an event (`catter/timers`
  enforces it; the README's "Waiting and teardown").
- An error in catter's logs fails the test unless it declares it:
  `test({ errors: ["<regular expression>"] }, ...)` (escape a message's
  `(`, `[`, `+`, `?`), only for an error the case provokes on purpose.
- Every test runs on a fresh copy of its case. Probes and experiments work
  on a copy too (`CATTER_KEEP_WORKSPACE=1` keeps a test's copy and prints
  where); never run catter in a case directory itself.
- A value that differs from run to run or machine to machine is normalized
  in `tools/testing/cli/run.ts` (`normalize`, `outerPlaces`), never by
  rewriting a test's commands to avoid it.

## Known pitfalls (each cost a real debugging session)

- Windows checkouts have CRLF line ends, and `box cp` brings them back:
  pull only the `snap.toml` files from a windows box and strip `\r`, or
  every input of the case shows as changed. `.gitattributes` keeps
  `tests/**` LF in git.
- `box push` sends tracked files and deletes nothing: a case removed here
  still runs on the box until it is removed there.
- Ninja prints a step when it finishes: a build whose output a block pins
  runs with `-j 1`, or its order changes from run to run. Commands a test
  runs side by side keep the order they started in (`t.cli` takes its
  block when it starts).
- The runner's environment reaches the build: variables that change a
  build tool's output (`MAKEFLAGS`, `CMAKE_GENERATOR`, `FORCE_COLOR`, …)
  are dropped in `run.ts` (`DROPPED`); a new one that leaks into output
  joins that list.
- On Windows every command runs in the developer environment of the newest
  Visual Studio (vcvars in a clean environment); MSVC's `cl` reads response
  files in the ANSI code page, so a response file naming the copy's
  non-ASCII root does not work there.
- On macOS, Apple's arm64e tools are killed by dyld when the hook is
  inserted (#115): every macOS build block carries that `wrong`; node-based
  capture cases still cover macOS.
- A debug (ASan) catter on Linux puts the sanitizer runtime first in the
  preload of every program the hook enters, with leak detection off for
  them (`src/catter-hook/unix/impl.cc`): a case needs nothing for it.
- A native stack trace never reaches stdout (only the log): one in a block
  is a regression.

## C++ unit tests (zest)

- `unittests/` mirrors `src/`; what belongs in a unit test rather than a
  case is in `tests/README.md`, "Unit tests".
- zest runs each test in a worker process with a 60 second limit; a suite
  that uses a fixed file or directory name runs alone:
  `ZEST_SUITE_ATTRS(serial = true);`. Debug a test in-process with
  `--no-isolation --test-filter=<suite>.<test>`.

## Framework code (`tools/testing`)

- Framework > mature library > handwritten: vitest's own concurrency over a
  custom pool, `diff` over a hand-rolled diff, `smol-toml` over a TOML
  parser of our own.
- `tools/` runs under node's strip-only TypeScript: erasable syntax only (no
  constructor parameter properties, no enums).
- Its own tests sit beside the code they test as `*.test.ts`;
  `pnpm --dir tools test` runs them, with no catter build.
