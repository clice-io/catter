# 从源码构建

## 前置要求

- [pixi](https://pixi.sh) -- 环境与依赖管理；其余的一切，包括 Bazel，都由它带来
- Git

## 支持的平台

| 平台           | 目标                       | 在哪里构建     |
| -------------- | -------------------------- | -------------- |
| Linux x64      | `x86_64-unknown-linux-gnu` | Linux          |
| macOS arm64    | `aarch64-apple-darwin`     | macOS          |
| Windows x64    | `x86_64-w64-mingw32`       | Linux、Windows |
| Windows on ARM | `aarch64-w64-mingw32`      | Linux、Windows |

每个平台都用 [xclang](https://github.com/clice-io/xclang) 23.1.3 的工具链构建，由 Bazel 下载：clang、lld 和 libc++，libc++ 链接进每个程序，所以程序只依赖系统自带的库。Linux 上的程序在 glibc 2.17 及以上运行，macOS 上的程序在 macOS 15 及以上运行。Windows 上的程序是 MinGW 的，每次 Windows 构建都会同时生成 x64 和 ARM64 两种架构的钩子 DLL。

## 构建命令

所有构建通过 pixi 任务管理：

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

程序生成在 `build/<type>/bin/bin`，彼此相邻，catter 就在这里找到它们。构建类型有两种：`RelWithDebInfo`，开启优化并带调试信息，即发布的版本；`Debug`，开启 AddressSanitizer，Windows on ARM 没有这种构建。

在 Windows 上，Bazel 默认的输出根目录对 Windows 路径来说太深，在 `%USERPROFILE%\.bazelrc` 中设置一个较短的：

```
startup --output_user_root=C:/b
```

## 构建系统

项目使用 [Bazel](https://bazel.build)，通过 npm 上的 bazelisk 运行（`pnpm exec bazel`），版本由 `.bazelversion` 指定；依赖来自 clice 的 Bazel registry [bazel.clice.io](https://bazel.clice.io)。C++ 标准为 C++23。`pixi run -e dev pnpm exec bazel run @compdb//:refresh` 为编辑器生成 `compile_commands.json`。

`src/support` 之上的 catter 代码是 C++20 具名模块：`src/command` 是 `catter.command`，其余是 `catter`。钩子库和它们共用的 `src/support` 仍是头文件。第三方库合为一个模块 `deps`，`modules/` 里存有 CI 构建的每种配置下的它；`scripts/modules.py` 用 `clice modularize` 从 `modules/demand.cpp` 生成它。

### 构建目标

- `bin/catter` -- 主 CLI 可执行文件（DECISION 守护进程）
- `bin/catter-proxy` -- 代理进程管理器，也是 wrapper 后端的 shim（包装程序）
- `hook_unix_payload` -- Unix 钩子共享库（Linux/macOS），即 `bin/libcatter-hook-unix.so` 或 `.dylib`
- `hook_x64_payload`、`hook_arm64_payload` -- Windows 钩子 DLL，即 `bin/catter-hook-x64.dll` 和 `bin/catter-hook-arm64.dll`，每种架构一个，每次 Windows 构建都会同时生成两者
- `support`、`command`、`session`、`capture`（以及 `capture_inject`）、`protocol`、`feature` -- 库，`src/` 下每个目录一个
- `dist` -- `pixi run -e dev build` 构建的内容；`package` 和 `symbols` -- 发布包及其调试符号

## 主要依赖

- [spdlog](https://github.com/gabime/spdlog) (1.15.3) -- 日志
- [kotatsu](https://github.com/clice-io/kotatsu) -- 异步运行时、测试框架、选项解析
- [Microsoft Detours](https://github.com/microsoft/Detours) -- Windows API 钩子与 DLL 注入（仅 Windows）
