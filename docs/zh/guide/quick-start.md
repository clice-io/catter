# 快速上手

## 前置条件

- 使用任意构建系统（Make、Ninja、CMake、Meson 等）的 C++ 项目

## 安装

Catter 通过 GitHub Releases 分发预编译包。前往 [GitHub Releases](https://github.com/clice-io/catter/releases) 页面，下载对应平台的压缩包，解压后将 `catter` 可执行文件所在目录加入 `PATH` 即可。

在 Linux 上，请解压到路径中没有空格和冒号的目录；在 macOS 上，路径中不能有冒号：把 catter 的钩子加载进构建的环境变量（`LD_PRELOAD`、`DYLD_INSERT_LIBRARIES`）会在这些字符处拆分路径，catter 拒绝从这样的目录运行构建。

预编译包当前支持的平台：

| 平台    | 架构  | 包                                                 |
| ------- | ----- | -------------------------------------------------- |
| Linux   | x64   | `catter-<version>.x86_64-unknown-linux-gnu.tar.gz` |
| macOS   | arm64 | `catter-<version>.aarch64-apple-darwin.tar.gz`     |
| Windows | x64   | `catter-<version>.x86_64-w64-mingw32.zip`          |
| Windows | arm64 | `catter-<version>.aarch64-w64-mingw32.zip`         |

次版本号为偶数的版本（`v0.2.0`）是稳定版，目前还没有。Nightly 是次版本号为奇数的预发布版本，版本号取自打 tag 时的 UTC 小时（`v0.1.2026102902` 即 2026-10-29 02:00 UTC）：`main` 每天只要有通过全部测试的新构建，就会发布一个 nightly，超过 30 天或排在最新 25 个之后的 nightly 会被删除，但最新的一个始终保留。

::: info
Catter 目前仍在积极开发中，请使用最新的 nightly。如果您的平台没有预编译包，可以参阅[从源码构建](../dev/build)自行编译。
:::

## 生成编译数据库

通过 catter 运行你的构建命令，即可捕获所有编译器调用：

```bash
catter compile-commands -o compile_commands.json -- make
```

这会在当前目录生成 `compile_commands.json` 文件，可直接用于任何语言服务器或静态分析工具。

## 命令格式

```
catter <command> [options] -- <build-command>
```

以上面的 CDB 示例为例：

| 部分                       | 含义                             |
| -------------------------- | -------------------------------- |
| `compile-commands`         | 命令：生成 CDB                   |
| `-o compile_commands.json` | 命令的选项：输出文件路径         |
| `--`                       | 命令的参数与构建命令之间的分隔符 |
| `make`                     | 实际要拦截的构建命令             |

`catter --help` 列出所有命令，`catter <command> --help` 显示某个命令的选项。

## 更多示例

### 命令树可视化

将捕获的构建命令 DAG 以 ASCII 树形式展示：

```bash
catter tree -- make
```

### 配合 CMake 使用

```bash
catter compile-commands -o compile_commands.json -- cmake --build build
```

### 你自己的 handler

把命令交给你自己编写的程序，即 [handler](../scripting/handlers.md)：

```bash
catter run --handler "python my_handler.py" -- cmake --build build
```
