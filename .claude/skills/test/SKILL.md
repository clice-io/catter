---
name: test
description: Run catter's test suites, on a box unless a local run is truly necessary; also how to test on macOS and Windows. Optional args = suite (unit | cases | all, default all), mode (debug | release | releasedbg, default debug, as the build skill's) and the box id the build ran on. Runs in a forked context; only a digest returns.
context: fork
---

Run the requested suites (default: all) against the requested build mode (default `debug`, the build skill's): the cases run the programs of that mode's build, so the build must be of the same mode and current.

## Where it runs

On the box that built catter (the build skill; the caller passes its id), not on this machine unless truly necessary; the framework's own tests need no build and run here. A box this skill leases because the caller passed none is reported with its id, like the build skill's, and never downed. The cases run the programs of a built tree: build first (`pixi run -e dev build` after `cfg <mode>`), then run.

Every platform tests on a box of its own OS, as it builds there (the build skill): `--os macos` for macOS, `--os windows` for Windows and `--os windows-arm` for Windows on ARM, where every pixi command goes through PowerShell:

```bash
box run $win 'pwsh -NoProfile -Command "Remove-Item Env:MSYSTEM -EA 0; pixi run -e dev cases-test debug"'
```

PowerShell sets a variable for the commands after it, not on the command line: `$env:CATTER_CASES = 'capture/'; pixi run -e dev cases-test debug`. When both MSVC and MinGW are built on one box, `cases-test` finds two catters: point the cases at one by its absolute path (the runner starts in `tests/`), `$env:CATTER_EXECUTABLE = "$PWD/build/mingw/x86_64/debug/catter.exe"; pixi run -e dev pnpm --dir tests run cases`, as CI's MinGW cases jobs do. On Windows on ARM a `debug` catter cannot start under the cases (0xC0000135): they run in vcvars64's environment, whose `PATH` has the x64 ASan runtime, not the arm64 `clang_rt.asan_dynamic-aarch64.dll` (from MSVC's `bin\Hostarm64\arm64`), which goes beside `catter.exe` first.

## Commands

- Unit tests: `pixi run -e dev unit-test`
- Cases: `pixi run -e dev cases-test [mode] [i/n]` (shard i of n, as CI's `cases` job runs them)
- Build, unit tests and cases in one go: `pixi run -e dev test [mode]`
- The framework's own tests, types and lint, which need no build: `pixi run -e dev check-testing`

Filtering specific tests:

- Unit: `build/<plat>/<arch>/<mode>/ut-catter --test-filter=<suite>.<test>` (`ut-common`, `ut-catter-hook-unix`, `ut-catter-hook-win` likewise)
- Cases: `CATTER_CASES=<path prefix> pixi run -e dev cases-test [mode]`, e.g. `CATTER_CASES=capture/` or `CATTER_CASES=builds/make/02_parallel`; the "Running" section of `tests/README.md` lists the rest.

This skill runs and reports — nothing else. Never fix code or tests from here, never set `UPDATE_SNAPSHOTS`. Fixing happens in the main conversation with full context.

Report back, per suite and platform: pass/fail and counts. For each failure: the test name (a case's is `<topic>/<subtopic>/<case> > <test>`), the assertion message or `snap.toml` diff excerpt (trimmed), and the exact filter command to reproduce it.
