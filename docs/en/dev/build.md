# Building from Source

## Prerequisites

- [pixi](https://pixi.sh) -- Environment and dependency management
- [XMake](https://xmake.io/) -- Build system (not installed by pixi; install separately)
- Git

## Supported Platforms

| Platform                              | Toolchain             |
| ------------------------------------- | --------------------- |
| Windows (win-64)                      | MSVC (VS2022)         |
| Linux (linux-64)                      | GCC 14.2              |
| macOS (osx-arm64)                     | xclang 23.1.2         |
| Windows (win-64), `mingw` environment | xclang 23.1.2 (MinGW) |
| Windows on ARM (win-64, emulated)     | MSVC, for ARM64       |

The `mingw` environment builds for Windows with [xclang](https://github.com/clice-io/xclang) instead of MSVC: `pixi run -e mingw cfg debug`, then `pixi run -e mingw build`.

Every Windows build also makes the hook DLL of the other architecture, x64 and ARM64 both: with MSVC, install the C++ build tools for ARM64 (and on Windows on ARM, those for x64) beside the default ones in the Visual Studio Installer.

## Build Commands

All builds are managed through pixi tasks:

```bash
# Configure the build (mode: debug, release, or releasedbg)
pixi run cfg debug

# Build the project
pixi run build

# Configure in release mode
pixi run cfg release
```

## Build System

The project uses [XMake](https://xmake.io/) as the build system, managed through pixi tasks. The C++ standard is C++23.

### Build Targets

- `catter` -- Main CLI executable (the DECISION daemon)
- `catter-proxy` -- Proxy process manager
- `catter-hook-unix` -- Unix hook shared library (Linux/macOS)
- `catter-hook-x64`, `catter-hook-arm64` -- Windows hook DLLs, one per architecture, both built by every Windows build
- `support` (with `support-resolver` and `support-winapi`), `command`, `session`, `capture` (with `capture-inject`), `protocol`, `feature` -- Static libraries, one per directory of `src/`

## Key Dependencies

- [spdlog](https://github.com/gabime/spdlog) (1.15.3) -- Logging
- [kotatsu](https://github.com/clice-io/kotatsu) -- Async runtime, testing framework, option parsing
- [Microsoft Detours](https://github.com/microsoft/Detours) -- Windows API hooking and DLL injection (Windows only)
