# Building from Source

## Prerequisites

- [pixi](https://pixi.sh) -- Environment and dependency management
- Git

## Supported Platforms

| Platform | Toolchain |
|---|---|
| Windows (win-64) | MSVC (VS2022) |
| Linux (linux-64) | GCC 14.2 |
| macOS (osx-arm64) | Clang 20.1 |

## Build Commands

All builds are managed through pixi tasks:

```bash
# Configure the build
pixi run cfg

# Build the project
pixi run build

# Build in debug mode
pixi run cfg-debug
pixi run build

# Build JavaScript/TypeScript API
pixi run build-js

# Install npm dependencies
pixi run pnpm-install
```

## Build System

The project uses [XMake](https://xmake.io/) as the build system, managed through pixi tasks. The C++ standard is C++23.

### Build Targets

- `catter` -- Main CLI executable (the DECISION daemon)
- `catter-proxy` -- Proxy process manager
- `catter-hook-unix` -- Unix hook shared library (Linux/macOS)
- `catter-hook-win64` -- Windows hook DLL
- `catter-core` -- Core library
- `common` -- Shared utilities library

## JavaScript Build

The TypeScript API under `api/` is compiled to JavaScript and embedded into the catter binary. Build with:

```bash
pixi run build-js
```

This runs rollup to bundle the TypeScript, and the resulting JS is compiled into the binary as a resource.

## Key Dependencies

- [QuickJS-ng](https://github.com/nicedoc/nicedoc.io) (v0.11.0) -- Embedded JavaScript engine
- [spdlog](https://github.com/gabime/spdlog) (1.15.3) -- Logging
- [kotatsu](https://github.com/clice-io/kotatsu) -- Async runtime, testing framework, option parsing
- [MinHook](https://github.com/TsudaKageworyo/MinHook) (v1.3.4) -- Windows API hooking (Windows only)
