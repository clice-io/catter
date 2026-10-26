---
name: test
description: Run catter's test suites, on a box unless a local run is truly necessary; also how to test on macOS and Windows. Optional args = suite (unit | cases | all, default all), build type (Debug | RelWithDebInfo, default RelWithDebInfo, as the build skill's) and the box id the build ran on. Runs in a forked context; only a digest returns.
context: fork
---

Run the requested suites (default: all) against the requested build type (default `RelWithDebInfo`, the build skill's): the suites run the programs of that type's build, `build/<type>/bin/bin`, so the build must be of the same type and current.

## Where it runs

On the box that built catter (the build skill; the caller passes its id), not on this machine unless truly necessary; the framework's own tests need no build and run here. A box this skill leases because the caller passed none is reported with its id, like the build skill's, and never downed. The suites run the programs of a built tree: build first (`pixi run -e dev build <type>`), then run.

Every platform tests on a box of its own OS: `--os macos` for macOS, `--os windows` for Windows and `--os windows-arm` for Windows on ARM, on the programs a Linux box built for them and `box cp` brought (the build skill); on Windows every pixi command goes through PowerShell, CI's default shell:

```bash
box run $win 'pwsh -NoProfile -Command "Remove-Item Env:MSYSTEM -EA 0; pixi run -e dev cases-test RelWithDebInfo"'
```

PowerShell sets a variable for the commands after it, not on the command line: `$env:CATTER_CASES = 'capture/'; pixi run -e dev cases-test RelWithDebInfo`.

## Commands

- Unit tests: `pixi run -e dev unit-test <type>`
- Cases: `pixi run -e dev cases-test <type> [i/n]` (shard i of n, as CI's `cases` job runs them)
- Build, unit tests and cases in one go: `pixi run -e dev test <type>`
- The framework's own tests, types and lint, which need no build: `pixi run -e dev check-testing`

Filtering specific tests:

- Unit: `build/<type>/bin/bin/unittests --test-filter=<suite>.<test>` (`unittests-hook-unix`, `unittests-hook-windows` likewise)
- Cases: `CATTER_CASES=<path prefix> pixi run -e dev cases-test <type>`, e.g. `CATTER_CASES=capture/` or `CATTER_CASES=builds/make/02_parallel`; the "Running" section of `tests/README.md` lists the rest.

This skill runs and reports — nothing else. Never fix code or tests from here, never set `UPDATE_SNAPSHOTS`. Fixing happens in the main conversation with full context.

Report back, per suite and platform: pass/fail and counts. For each failure: the test name (a case's is `<topic>/<subtopic>/<case> > <test>`), the assertion message or `snap.toml` diff excerpt (trimmed), and the exact filter command to reproduce it.
