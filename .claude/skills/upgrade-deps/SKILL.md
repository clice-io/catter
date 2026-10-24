---
name: upgrade-deps
description: Read before bumping xclang (the `llvm-option-inc` and `xclang` pins in pixi.toml) or kotatsu (its package in xmake.lua) — what moves together, what to regenerate, how to verify. Arg = the new xclang version (e.g. 23.1.3.3) or kotatsu commit.
---

# Upgrading xclang and kotatsu

Both come from clice-io and move often; each bump is its own PR on a
branch `build/xclang-<version>` or `build/kotatsu-<version>`, titled
`build: xclang <version>` / `build: kotatsu <version>`.

## xclang

One [xclang](https://github.com/clice-io/xclang) release, tagged
`<llvm version>.<revision>` (`23.1.2.4`: LLVM 23.1.2, xclang's fourth build
of it), brings two conda packages (channel `https://conda.clice.io`):

- `llvm-option-inc` — LLVM's TableGen output for the option tables of
  clang, lld (COFF, ELF, MachO, MinGW, wasm), llvm-lib and llvm-dlltool,
  which `src/common/option` includes (`<llvm-options-td/*.inc>`);
- `xclang` — the toolchain: the clang and lld that build catter on macOS
  and for MinGW.

They always move together:

1. The release exists: `gh release view <VERSION> -R clice-io/xclang`.
2. `pixi.toml`: both pins `==<VERSION>`; then `pixi lock`.
3. A new LLVM version, not only a new revision, changes the option tables.
   The TypeScript option enums (`api/src/option/*.ts`) mirror the C++
   tables' ID order and are regenerated from the new package:
   `pixi run -e dev node scripts/js-gen/generate_llvm_option.mjs`. An
   option catter names that was renamed or removed upstream shows up as a
   C++ build error or a tsc error; the unit tests under
   `unittests/common/opt` and the `tests/inspect` cases pin the parsing.
4. Build and run the unit tests and the cases on every CI leg (the build
   and test skills): Linux,
   macOS and Windows MSVC and MinGW, debug and release. macOS and MinGW
   are the legs xclang's clang compiles.

## kotatsu

`xmake.lua` defines the package: `package("kotatsu")` with one
`add_versions("<n>", "<sha>")`, where `<n>` is `git rev-list --count <sha>`
in a full clone of clice-io/kotatsu (a shallow clone counts wrong), and the
configs catter consumes (`async`, `deco`, `ztest` on, `http` off).

1. Pick the commit, usually kotatsu's `main`; replace the version line.
2. Keep the package's `add_deps` (libuv, cpptrace) at the versions
   kotatsu's own `xmake.lua` requires at that commit.
3. Read kotatsu's history since the old pin
   (`git log --oneline <old>..<new>`) for API changes, adapt catter to
   them, and list them in the PR body.
4. Build and run the unit tests and the cases on every CI leg, debug and
   release (the build and test skills): the cases are what exercises
   kotatsu's IPC, pipes and processes. A new version is in no package cache: CI and every box install
   it first.

catter and clice pin kotatsu separately (clice through Bazel), so the two
need not move together.
