---
name: upgrade-deps
description: Read before bumping xclang (its `bazel_dep` in MODULE.bazel and its pin in pixi.toml), kotatsu or another library of `modules/` (their `bazel_dep` in MODULE.bazel), or when catter starts to use a library header or macro `modules/` does not export yet — what moves together, how to regenerate `modules/`, how to verify. Arg = the new xclang version (e.g. 23.1.3.5) or kotatsu version.
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
  (`"llvm-options-td/*.inc"`);
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
4. Regenerate `modules/` (below): libc++, the C libraries and the
   compiler's own headers are in every configuration of it.
5. Build and run the unit tests and the cases on everything CI builds (the
   build and test skills): Linux, macOS and Windows x64, `Debug` and
   `RelWithDebInfo`, and the Windows ARM64 `RelWithDebInfo`.

## kotatsu

`MODULE.bazel`: `bazel_dep(name = "kotatsu", version = "0.1.0.<n>")`, the
module kotatsu's CI publishes to the registry for the n-th commit of its
`main` (`git rev-list --count <sha>` in a full clone; a shallow clone counts
wrong). Its own `MODULE.bazel` names the libraries it builds on (libuv,
cpptrace, simdjson, ...) at registry versions, which Bazel resolves with
catter's own (`cpptrace`, `spdlog`): a newer one of theirs is taken for
both. The kotatsu libraries catter's modules use are in `LIBRARIES` of
`modules/modules.bzl` (`@kotatsu//:fs`, `:sys`, `:async`, `:ipc`,
`:codec_json`, `:deco`, `:zest`, ...), which `deps` wraps; `support` and
the hook payloads name theirs in `BUILD.bazel`, the payloads `:fs` and
`:sys`, and the Windows one `:ipc_codec` too.

1. Pick the version, usually that of kotatsu's `main`; it is in the
   registry once `modules/kotatsu/<version>/` is on clice-io/bazel's
   `main`.
2. Read kotatsu's history since the old version
   (`git log --oneline <old>..<new>`) for API changes and for libraries
   its `BUILD.bazel` split or renamed, adapt catter to them, and list them
   in the PR body.
3. Regenerate `modules/` (below).
4. Build and run the unit tests and the cases on everything CI builds,
   `Debug` and `RelWithDebInfo` (the build and test skills): the cases are
   what exercises kotatsu's IPC, pipes and processes.

catter and clice pin kotatsu separately, so the two need not move
together.

## Regenerating `modules/`

`modules/` is the third-party libraries as one C++20 module, `deps`, and
the prelude every module unit of catter includes first, written by `clice
modularize` from `modules/demand.cpp`, which records what catter takes
from them: their headers, the C library names only those headers bring, and
the macros catter uses, an `#ifdef` each. Its headers differ by platform
and build type, so it is generated for each of the seven configurations CI
builds and merged; nothing under `modules/` but `demand.cpp`, `BUILD.bazel`
and `modules.bzl` is edited by hand. A bump of xclang or of a library it
wraps (kotatsu, spdlog, cpptrace), and catter starting to use a header or
macro `deps` does not export, regenerate it: the build stops on a name or
macro it does not know, which goes into `demand.cpp` first (under `#ifdef
_WIN32` or `#ifndef _WIN32` when one platform alone uses it). A new library
goes into `LIBRARIES` in `modules/modules.bzl`, and its repository into the
libraries' globs of `scripts/modules.py`.

`scripts/modules.py` builds `demand.cpp` for a configuration, indexes it
with clice and wraps the libraries' headers, then merges the
configurations. It runs any clice release, the one of the box's platform
(`clice-<version>.<triple>.tar.gz` of
[clice's releases](https://github.com/clice-io/clice/releases)), on a Linux
and a macOS box with the tree pushed:

```bash
# On each box: clice in ~/clice
curl -fsSL https://github.com/clice-io/clice/releases/download/<tag>/clice-<version>.<triple>.tar.gz | tar xzf - -C ~
# Linux box: its own configurations and the Windows ones
pixi run -e dev python scripts/modules.py --clice ~/clice/bin/clice generate \
    linux-x64 linux-x64-debug windows-x64 windows-x64-debug windows-arm64
# macOS box
pixi run -e dev python scripts/modules.py --clice ~/clice/bin/clice generate \
    macos-arm64 macos-arm64-debug
# here: the macOS wrappings to the Linux box, the merge, the result back
box cp $mac $linux build/modules/macos-arm64 build/modules/macos-arm64-debug
box run $linux 'pixi run -e dev python scripts/modules.py --clice ~/clice/bin/clice merge'
box cp $linux . modules
```

A warning of `clice modularize` stops `generate`: a header no glob of
`scripts/modules.py` claims means a library's include directory moved. The diff of `modules/` is
part of the review: names the libraries dropped or renamed, and macros
that changed.
