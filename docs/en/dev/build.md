# Building from Source

## Prerequisites

- [pixi](https://pixi.sh) -- Environment and dependency management
- [XMake](https://xmake.io/) -- Build system (not installed by pixi; install separately)
- Git

## Supported Platforms

| Platform                          | Toolchain                         |
| --------------------------------- | --------------------------------- |
| Linux (linux-64)                  | GCC 14.2                          |
| macOS (osx-arm64)                 | xclang 23.1.3                     |
| Windows (win-64)                  | xclang 23.1.3, for MinGW          |
| Windows on ARM (win-64, emulated) | xclang 23.1.3, for MinGW on ARM64 |

Windows builds with [xclang](https://github.com/clice-io/xclang) for MinGW (`x86_64-w64-mingw32`); `pixi run cfg debug arm64` configures for ARM64 (`aarch64-w64-mingw32`) instead, on Windows on ARM, or on x64 Windows, which cannot run what it builds. Every Windows build also makes the hook DLL of the other architecture, x64 and ARM64 both.

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
