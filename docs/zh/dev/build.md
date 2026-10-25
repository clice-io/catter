# 从源码构建

## 前置要求

- [pixi](https://pixi.sh) -- 环境与依赖管理
- [XMake](https://xmake.io/) -- 构建系统（pixi 不会安装，需单独安装）
- Git

## 支持的平台

| 平台                           | 工具链                 |
| ------------------------------ | ---------------------- |
| Windows (win-64)               | MSVC (VS2022)          |
| Linux (linux-64)               | GCC 14.2               |
| macOS (osx-arm64)              | xclang 23.1.2          |
| Windows (win-64)，`mingw` 环境 | xclang 23.1.2（MinGW） |

`mingw` 环境在 Windows 上用 [xclang](https://github.com/clice-io/xclang) 代替 MSVC 构建：先 `pixi run -e mingw cfg debug`，再 `pixi run -e mingw build`。

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
- `catter-hook-win64` -- Windows 钩子 DLL
- `catter-core` -- 核心库
- `common` -- 共享工具库

## 主要依赖

- [spdlog](https://github.com/gabime/spdlog) (1.15.3) -- 日志
- [kotatsu](https://github.com/clice-io/kotatsu) -- 异步运行时、测试框架、选项解析
- [MinHook](https://github.com/TsudaKageworyo/MinHook) (v1.3.4) -- Windows API 钩子（仅 Windows）
