---
name: build
description: Build catter and run its unit tests, on a box unless a local build is truly necessary; also how to build on macOS and Windows. Optional args = platform (linux | macos | windows | windows-arm, default linux), mode (debug | release | releasedbg, default debug) and the caller's box id. Runs in a forked context; only the outcome returns.
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

- Every platform builds on a box of its own OS, as CI does: `--os macos` for macOS, `--os windows` for Windows. Windows on ARM (CI's `windows-arm64` job) is cross-built on the `--os windows` box, and its programs run on a `--os windows-arm` box, copied there with `box cp` (below); nothing else is copied between boxes.
- A box restores the xmake package cache of CI's debug builds: a `release` or `releasedbg` configure there installs its packages first (minutes), since debug packages carry the sanitizer configs.
- One box runs one build at a time: two `ci-build`s in one `~/src` reconfigure each other's tree (`cfg` is `xmake config --clean`). x64 and ARM64 on one Windows box, or debug and release on one box, run one after the other; for parallel runs, lease another box.
- A session may hold several boxes, one per platform, each leased once and reused, building in parallel. Pushing never waits for a box: CI has the capacity to verify in parallel.

### Windows

`box run` runs in Git Bash, which makes xmake take the host for MSYS (`msys is not a valid platform`); unsetting `MSYSTEM` in bash does not help. Run every pixi and xmake command through PowerShell; the programs it builds run from Git Bash as they are:

```bash
win=$(box up clice-io/catter --os windows)
box push $win
box run $win 'pwsh -NoProfile -Command "Remove-Item Env:MSYSTEM -EA 0; pixi run -e dev ci-build debug"'
```

Windows on ARM: x64 Windows builds it and cannot run it, so the build there is the build alone (`ci-cross-build`), and the unit tests run on Windows on ARM (the test skill), as CI's `unit` job does. ARM64 has no ASan: its `debug` build is unsanitized.

```bash
box run $win 'pwsh -NoProfile -Command "Remove-Item Env:MSYSTEM -EA 0; pixi run -e dev ci-cross-build release"'
arm=$(box up clice-io/catter --os windows-arm)
box push $arm
box cp $win $arm build/mingw/arm64/release
```

## Commands

All from the repo root, in the environment CI uses (`-e dev`):

- Everything a CI build job runs: `pixi run -e dev ci-build <mode>` = `cfg <mode>`, `build`, the unit tests, then `check-protocol` (the protocol's schema and the SDKs' generated types are what the build gives). The snapshot cases run on the built programs: the test skill.
- Step by step: `pixi run -e dev cfg <mode> [toolchain]` (`xmake config --clean`: a full reconfigure; the toolchain defaults to the OS's: gcc, clang; on Windows `cfg <mode> [arch]`, `x86_64` or `arm64`, both with xclang for MinGW), `pixi run -e dev build` (all default targets, the test programs included).
- The unit tests alone, on a built tree: `pixi run -e dev unit-test` (`xmake test`).
- One unit test in the binary's own process, for debugging: `build/<plat>/<arch>/<mode>/unittests --no-isolation --test-filter=<suite>.<test>` (`unittests-hook-unix`, `unittests-hook-windows` likewise; `--help` lists the rest).
- Release packages: `pixi run -e dev package` after a `release` build writes `release-assets/catter-<version>-<plat>-<arch>.*`, as CI's packaging step does.
- `compile_commands.json` for catter's own sources: `build/compile_commands.json`, kept up to date by every build.

On failure:

- Mechanical breakage (missing include, renamed symbol, stale call site after an agreed-on change): fix it, rebuild, and list every file you touched in the report.
- Design-level errors (the fix requires a decision): do not guess — report the error with `file:line` and the relevant excerpt.
- A failing test is reported, never fixed from here: the test name, the failed assertion (trimmed), and the command that reruns it alone.

Report back: platform, mode, success or failure, files changed (if any), per suite pass/fail and counts, and for failures a digested error list — never the raw compiler spew.
