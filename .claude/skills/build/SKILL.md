---
name: build
description: Build catter and run its current suites, on a box unless a local build is truly necessary; also how to build on macOS and Windows. Optional args = platform (linux | macos | windows | mingw, default linux), mode (debug | release | releasedbg, default debug) and the caller's box id. Runs in a forked context; only the outcome returns.
context: fork
---

Build the project for the requested platform and mode (default: linux, `debug`), as CI does.

## Where it runs

On a box, not on this machine: this machine hosts every agent's session, and one build saturates it. Build here only when truly necessary. The agent-box skill covers the tool itself (leases, limits, safety); for catter:

```bash
id=$(box up clice-io/catter)                    # ~1 min; only when the caller passed no id
box push $id                                    # tracked files only: `git add` new ones first
box run $id 'pixi run -e dev ci-build debug'    # run_in_background
```

The box belongs to the calling session, not to this skill: build on the id the caller passed, or lease one and report its id. Never `box down` it — the session reuses it, warm, for every later push, and an idle box stops by itself after an hour.

- Every platform builds on a box of its own OS, as CI does: `--os macos` for macOS, `--os windows` for both Windows environments (MSVC and MinGW). Nothing cross-builds, and nothing is copied between boxes.
- A box restores the xmake package cache of CI's debug legs: a `release` or `releasedbg` configure there installs its packages first (minutes), since debug packages carry the sanitizer configs.
- A session may hold several boxes, one per platform, each leased once and reused, building in parallel. Pushing never waits for a box: CI has the capacity to verify in parallel.

### Windows

`box run` runs in Git Bash, which makes xmake take the host for MSYS (`msys is not a valid platform`); unsetting `MSYSTEM` in bash does not help. Run every pixi and xmake command through PowerShell; the programs it builds run from Git Bash as they are:

```bash
win=$(box up clice-io/catter --os windows)
box run $win 'pwsh -NoProfile -Command "Remove-Item Env:MSYSTEM -EA 0; pixi run -e dev ci-build debug"'     # MSVC
box run $win 'pwsh -NoProfile -Command "Remove-Item Env:MSYSTEM -EA 0; pixi run -e mingw ci-build debug"'   # MinGW
```

## Commands

All from the repo root, in the environment CI uses (`-e dev`; `-e mingw` for MinGW on Windows):

- Everything CI runs on one leg: `pixi run -e dev ci-build <mode>` = `npm-install`, `cfg <mode>`, `build`, then the unit tests and the lit suite.
- Step by step: `pixi run -e dev npm-install` (the pnpm workspace, needed by the JS build), `pixi run -e dev cfg <mode> [toolchain]` (`xmake config --clean`: a full reconfigure; the toolchain defaults to the OS's: gcc, clang, msvc), `pixi run -e dev build` (all default targets, the test programs included).
- The suites alone, on a built tree: `pixi run -e dev unit-test` (`xmake test`), `pixi run -e dev integration-test` (lit, one test at a time: every catter process shares one IPC pipe).
- One unit test in the binary's own process, for debugging: `build/<plat>/<arch>/<mode>/ut-catter --no-isolation --test-filter=<suite>.<test>` (`ut-common`, `ut-catter-hook-unix`, `ut-catter-hook-win64` likewise; `--help` lists the rest).
- Release packages: `pixi run -e dev package` after a `release` build writes `release-assets/catter-<version>-<plat>-<arch>.*`, as CI's packaging step does.
- `compile_commands.json` for catter's own sources: `build/compile_commands.json`, kept up to date by every build.

On failure:

- Mechanical breakage (missing include, renamed symbol, stale call site after an agreed-on change): fix it, rebuild, and list every file you touched in the report.
- Design-level errors (the fix requires a decision): do not guess — report the error with `file:line` and the relevant excerpt.
- A failing test is reported, never fixed from here: the test name, the assertion or FileCheck mismatch (trimmed), and the command that reruns it alone.

Report back: platform, mode, success or failure, files changed (if any), per suite pass/fail and counts, and for failures a digested error list — never the raw compiler spew.
