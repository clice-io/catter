# 从源码构建

## 前置要求

- [pixi](https://pixi.sh) -- 环境与依赖管理
- Git

## 支持的平台

| 平台 | 工具链 |
|---|---|
| Windows (win-64) | MSVC (VS2022) |
| Linux (linux-64) | GCC 14.2 |
| macOS (osx-arm64) | Clang 20.1 |

## 构建命令

所有构建通过 pixi 任务管理：

```bash
# 配置构建
pixi run cfg

# 构建项目
pixi run build

# 以 debug 模式构建
pixi run cfg-debug
pixi run build

# 构建 JavaScript/TypeScript API
pixi run build-js

# 安装 npm 依赖
pixi run pnpm-install
```

## 构建系统

项目使用 [XMake](https://xmake.io/) 作为构建系统，通过 pixi 任务管理。C++ 标准为 C++23。

### 构建目标

- `catter` -- 主 CLI 可执行文件（DECISION 守护进程）
- `catter-proxy` -- 代理进程管理器
- `catter-hook-unix` -- Unix hook 共享库（Linux/macOS）
- `catter-hook-win64` -- Windows hook DLL
- `catter-core` -- 核心库
- `common` -- 共享工具库

## JavaScript 构建

`api/` 下的 TypeScript API 会被编译为 JavaScript 并嵌入到 catter 二进制文件中。构建命令：

```bash
pixi run build-js
```

该命令通过 rollup 打包 TypeScript，生成的 JS 文件会作为资源编译进二进制文件。

## 主要依赖

- [QuickJS-ng](https://github.com/quickjs-ng/quickjs) (v0.11.0) -- 内嵌 JavaScript 引擎
- [spdlog](https://github.com/gabime/spdlog) (1.15.3) -- 日志
- [kotatsu](https://github.com/clice-io/kotatsu) -- 异步运行时、测试框架、选项解析
- [MinHook](https://github.com/TsudaKageworyo/MinHook) (v1.3.4) -- Windows API 钩子（仅 Windows）
