---
name: build
description: Build catter and run its unit tests, on a box unless a local build is truly necessary; also how to build for macOS and Windows. Optional args = platform (linux | macos | windows | windows-arm, default linux), build type (Debug | RelWithDebInfo, default RelWithDebInfo) and the caller's box id. Runs in a forked context; only the outcome returns.
context: fork
---

Build the project for the requested platform and build type (default: linux, `RelWithDebInfo`), as CI does.

## Where it runs

On a box, not on this machine: this machine hosts every agent's session, and one build saturates it. Build here only when truly necessary. The agent-box skill covers the tool itself (leases, limits, safety); for catter:

```bash
id=$(box up clice-io/catter)                            # ~1 min; only when the caller passed no id
box push $id                                            # tracked files only: `git add` new ones first
box run $id 'pixi run -e dev build RelWithDebInfo'      # run_in_background
```

The box belongs to the calling session, not to this skill: build on the id the caller passed, or lease one and report its id. Never `box down` it — the session reuses it, warm, for every later push, and an idle box stops by itself after an hour.

- Linux and macOS build on a box of their own OS (`--os macos`), as CI does. The Windows programs, x64 and ARM64, build on a Linux box, with xclang's toolchain for the target, and run on a box of their own (`--os windows`, `--os windows-arm`), copied there with `box cp` (below); a Windows box builds nothing.
- A box restores the Bazel caches of CI's last `main` run of `RelWithDebInfo` (`user.bazelrc` in `~/src` points Bazel at them; on Linux also Windows x64's): a `Debug` build, or a build of another target, compiles what the cache lacks first.
- One box runs one build at a time. Each target platform builds into an output tree of its own, and `build/<type>/bin` points at the last build's: after a Windows build on a Linux box, `build/<type>/bin/bin` holds the Windows programs alone, and a Linux build points it back. For Linux and Windows in parallel, lease another Linux box.
- A session may hold several boxes, one per platform, each leased once and reused, building in parallel. Pushing never waits for a box: CI has the capacity to verify in parallel.

### Windows

Build on a Linux box for the target, then bring the programs to a Windows box, which runs them from Git Bash as they are, and every pixi command through PowerShell, as CI's default shell:

```bash
box run $lin 'pixi run -e dev build RelWithDebInfo -- --platforms=@xclang//platforms:x86_64-w64-mingw32'
win=$(box up clice-io/catter --os windows)
box push $win
box cp $lin $win build/RelWithDebInfo/bin/bin --exclude '*.params' --exclude '*.stripped' --exclude '*.gsymutil.sh'
box run $win 'pwsh -NoProfile -Command "Remove-Item Env:MSYSTEM -EA 0; pixi run -e dev unit-test RelWithDebInfo"'
```

Windows on ARM: `aarch64-w64-mingw32` in place of `x86_64-w64-mingw32`, and a `--os windows-arm` box, as CI's `windows-arm64` job. It has no ASan: xclang refuses its `Debug` build.

## Commands

All from the repo root, in the environment CI uses (`-e dev`):

- The build: `pixi run -e dev build <type>` builds `//:dist` — the programs, the hook payloads, the unit tests and `catter-protocol-schema` — into `build/<type>/bin/bin` (`.bazelrc`). `Debug` is the ASan build (`-c dbg --features=asan`), but for the hook payloads, which never are (`bazel/defs.bzl`); `RelWithDebInfo` (`-c opt --copt=-g`) is what releases ship. Arguments after `--` go to Bazel: `--platforms=@xclang//platforms:<triple>` builds for another target, `//:package //:symbols` the release package and its symbols (`build/<type>/bin/catter.tar.gz`, `catter.zip` for Windows, and `catter-symbol.*`), as CI's packaged builds do.
- The unit tests, on a built tree: `pixi run -e dev unit-test <type>`, the programs of `build/<type>/bin/bin` with no arguments, as CI's `unit` job runs them.
- One unit test in the binary's own process, for debugging: `build/<type>/bin/bin/unittests --no-isolation --test-filter=<suite>.<test>` (`unittests-hook-unix`, `unittests-hook-windows` likewise; `--help` lists the rest).
- The protocol's schema and the SDKs' types: `pixi run -e dev check-protocol` (`protocol` writes them), which CI's Linux `RelWithDebInfo` build runs.
- The package's dependencies: `pixi run -e dev python scripts/check_artifact_deps.py build/RelWithDebInfo/bin/catter.tar.gz` (`.zip` for Windows), which CI's packaged builds run.
- `compile_commands.json` for catter's own sources: `pixi run -e dev pnpm exec bazel run @compdb//:refresh`, which writes it at the repo root.

On failure:

- Mechanical breakage (missing include or import, renamed symbol, stale call site after an agreed-on change): fix it, rebuild, and list every file you touched in the report. A module unit that does not know a name or macro of the standard library or of a library of `modules/` needs it in `modules/demand.cpp` and `modules/` regenerated (the upgrade-deps skill), never an include of its header.
- Design-level errors (the fix requires a decision): do not guess — report the error with `file:line` and the relevant excerpt.
- A failing test is reported, never fixed from here: the test name, the failed assertion (trimmed), and the command that reruns it alone.

Report back: platform, build type, success or failure, files changed (if any), per suite pass/fail and counts, and for failures a digested error list — never the raw compiler spew.
