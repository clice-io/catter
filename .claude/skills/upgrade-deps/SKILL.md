---
name: upgrade-deps
description: Read before bumping xclang (its `bazel_dep` in MODULE.bazel and its pin in pixi.toml) or kotatsu (its `bazel_dep` in MODULE.bazel) — what moves together, what to regenerate, how to verify. Arg = the new xclang version (e.g. 23.1.3.5) or kotatsu version.
---

# Upgrading xclang and kotatsu

Both come from clice-io and move often; each bump is its own PR on a
branch `build/xclang-<version>` or `build/kotatsu-<version>`, titled
`build: xclang <version>` / `build: kotatsu <version>`. Both are modules of
the clice Bazel registry, [bazel.clice.io](https://bazel.clice.io)
(clice-io/bazel), which `.bazelrc` names.

## xclang

One [xclang](https://github.com/clice-io/xclang) release, tagged
`<llvm version>.<revision>` (`23.1.2.4`: LLVM 23.1.2, xclang's fourth build
of it), is in two places, which always move together:

- `MODULE.bazel`: `bazel_dep(name = "xclang", version = ...)`, the Bazel
  module of the release: the toolchain that builds catter on every
  platform, and `@llvm_option_inc`, LLVM's TableGen output for the option
  tables of clang, lld (COFF, ELF, MachO, MinGW, wasm), llvm-lib and
  llvm-dlltool, which `src/command/option` includes
  (`<llvm-options-td/*.inc>`);
- `pixi.toml`: `xclang = "==<version>"`, the conda package (channel
  `https://conda.clice.io`), for the LLVM tools
  `scripts/check_artifact_deps.py` runs.

1. The release exists, and its module is in the registry:
   `gh release view <VERSION> -R clice-io/xclang`,
   `curl -sf https://bazel.clice.io/modules/xclang/<VERSION>/MODULE.bazel`.
2. Both versions `<VERSION>`; then `pixi lock`.
3. A new LLVM version, not only a new revision, changes the option tables.
   An option catter names that was renamed or removed upstream shows up as
   a C++ build error; the unit tests under
   `unittests/command/option` and the `tests/explain` cases pin the parsing.
4. Build and run the unit tests and the cases on everything CI builds (the
   build and test skills): Linux, macOS and Windows x64, `Debug` and
   `RelWithDebInfo`, and the Windows ARM64 `RelWithDebInfo`.

## kotatsu

`MODULE.bazel`: `bazel_dep(name = "kotatsu", version = "0.1.0.<n>")`, the
module kotatsu's CI publishes to the registry for the n-th commit of its
`main` (`git rev-list --count <sha>` in a full clone; a shallow clone counts
wrong). Its own `MODULE.bazel` names the libraries it builds on (libuv,
cpptrace, simdjson, ...) at registry versions, which Bazel resolves with
catter's own (`cpptrace`, `spdlog`): a newer one of theirs is taken for
both. catter's targets name the kotatsu libraries they use (`@kotatsu//:fs`,
`:sys`, `:async`, `:ipc`, `:codec_json`, `:deco`, `:zest`, ...); the hook
payloads `:fs` and `:sys` alone.

1. Pick the version, usually that of kotatsu's `main`; it is in the
   registry once `modules/kotatsu/<version>/` is on clice-io/bazel's
   `main`.
2. Read kotatsu's history since the old version
   (`git log --oneline <old>..<new>`) for API changes and for libraries
   its `BUILD.bazel` split or renamed, adapt catter to them, and list them
   in the PR body.
3. Build and run the unit tests and the cases on everything CI builds,
   `Debug` and `RelWithDebInfo` (the build and test skills): the cases are
   what exercises kotatsu's IPC, pipes and processes.

catter and clice pin kotatsu separately, so the two need not move
together.
