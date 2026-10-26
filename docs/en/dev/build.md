# Building from Source

## Prerequisites

- [pixi](https://pixi.sh) -- Environment and dependency management; it brings everything else, Bazel included
- Git

## Supported Platforms

| Platform       | Target                     | Built on       |
| -------------- | -------------------------- | -------------- |
| Linux x64      | `x86_64-unknown-linux-gnu` | Linux          |
| macOS arm64    | `aarch64-apple-darwin`     | macOS          |
| Windows x64    | `x86_64-w64-mingw32`       | Linux, Windows |
| Windows on ARM | `aarch64-w64-mingw32`      | Linux, Windows |

Every platform builds with the toolchain of [xclang](https://github.com/clice-io/xclang) 23.1.3, which Bazel downloads: clang, lld and libc++, linked into each program, so a program depends on the system's libraries alone. Linux programs run on glibc 2.17 and later, macOS programs on macOS 15 and later. The Windows programs are MinGW's, and every Windows build makes the hook DLLs of both architectures, x64 and ARM64.

## Build Commands

All builds are managed through pixi tasks:

```bash
# Build the programs, the hook libraries and the unit tests (RelWithDebInfo)
pixi run -e dev build

# The ASan build
pixi run -e dev build Debug

# Build for Windows on ARM from Linux or Windows x64
pixi run -e dev build RelWithDebInfo -- --platforms=@xclang//platforms:aarch64-w64-mingw32

# The release package, build/RelWithDebInfo/bin/catter.tar.gz (catter.zip on Windows)
pixi run -e dev build RelWithDebInfo -- //:package
```

The programs are built into `build/<type>/bin/bin`, side by side as catter finds them. The build types are `RelWithDebInfo`, optimized with debug information, what releases ship, and `Debug`, with AddressSanitizer, which Windows on ARM has not.

On Windows, Bazel's default output root is too deep for Windows paths; put a short one in `%USERPROFILE%\.bazelrc`:

```
startup --output_user_root=C:/b
```

## Build System

The project uses [Bazel](https://bazel.build), run through bazelisk from npm (`pnpm exec bazel`) at the version of `.bazelversion`; its dependencies come from the clice Bazel registry, [bazel.clice.io](https://bazel.clice.io). The C++ standard is C++23. `pixi run -e dev pnpm exec bazel run @compdb//:refresh` writes `compile_commands.json` for editors.

catter's code above `src/support` is C++20 named modules: `src/command` is `catter.command`, the rest `catter`. The hook payloads, and `src/support`, which they share, stay headers. The third-party libraries are one module, `deps`, which `modules/` holds for every configuration CI builds; `scripts/modules.py` writes it with `clice modularize` from `modules/demand.cpp`.

### Build Targets

- `bin/catter` -- Main CLI executable (the DECISION daemon)
- `bin/catter-proxy` -- Proxy process manager
- `hook_unix_payload` -- Unix hook shared library (Linux/macOS), `bin/libcatter-hook-unix.so` or `.dylib`
- `hook_x64_payload`, `hook_arm64_payload` -- Windows hook DLLs, `bin/catter-hook-x64.dll` and `bin/catter-hook-arm64.dll`, one per architecture, both built by every Windows build
- `support`, `command`, `session`, `capture` (with `capture_inject`), `protocol`, `feature` -- Libraries, one per directory of `src/`
- `dist` -- what `pixi run -e dev build` builds; `package` and `symbols` -- the release package and its debug symbols

## Key Dependencies

- [spdlog](https://github.com/gabime/spdlog) (1.15.3) -- Logging
- [kotatsu](https://github.com/clice-io/kotatsu) -- Async runtime, testing framework, option parsing
- [Microsoft Detours](https://github.com/microsoft/Detours) -- Windows API hooking and DLL injection (Windows only)
