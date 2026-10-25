# 从源码构建

## 前置要求

- [pixi](https://pixi.sh) -- 环境与依赖管理
- [XMake](https://xmake.io/) -- 构建系统（pixi 不会安装，需单独安装）
- Git

## 支持的平台

| 平台                               | 工具链                             |
| ---------------------------------- | ---------------------------------- |
| Linux (linux-64)                   | GCC 14.2                           |
| macOS (osx-arm64)                  | xclang 23.1.3                      |
| Windows (win-64)                   | xclang 23.1.3，构建 MinGW          |
| Windows on ARM（win-64，模拟运行） | xclang 23.1.3，构建 ARM64 的 MinGW |

Windows 上用 [xclang](https://github.com/clice-io/xclang) 构建 MinGW 版本（`x86_64-w64-mingw32`）；`pixi run cfg debug arm64` 则配置为构建 ARM64 版本（`aarch64-w64-mingw32`），可以在 Windows on ARM 上进行，也可以在 x64 Windows 上进行，只是 x64 Windows 运行不了构建出的程序。每次 Windows 构建都会同时构建另一种架构的钩子 DLL，x64 和 ARM64 两个都有。

## 构建命令

所有构建通过 pixi 任务管理：

```bash
# Configure the build (mode: debug, release, or releasedbg)
pixi run cfg debug

# Build the project
pixi run build

# Configure in release mode
pixi run cfg release
```

## 构建系统

项目使用 [XMake](https://xmake.io/) 作为构建系统，通过 pixi 任务管理。C++ 标准为 C++23。

### 构建目标

- `catter` -- 主 CLI 可执行文件（DECISION 守护进程）
- `catter-proxy` -- 代理进程管理器
- `catter-hook-unix` -- Unix 钩子共享库（Linux/macOS）
- `catter-hook-x64`、`catter-hook-arm64` -- Windows 钩子 DLL，每种架构一个，Windows 上的每次构建都会同时生成两者
- `support`（以及 `support-resolver` 和 `support-winapi`）、`command`、`session`、`capture`（以及 `capture-inject`）、`protocol`、`feature` -- 静态库，`src/` 下每个目录一个

## 主要依赖

- [spdlog](https://github.com/gabime/spdlog) (1.15.3) -- 日志
- [kotatsu](https://github.com/clice-io/kotatsu) -- 异步运行时、测试框架、选项解析
- [Microsoft Detours](https://github.com/microsoft/Detours) -- Windows API 钩子与 DLL 注入（仅 Windows）
