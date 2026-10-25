# 快速上手

## 前置条件

- 使用任意构建系统（Make、Ninja、CMake、Meson 等）的 C++ 项目

## 安装

Catter 通过 GitHub Releases 分发预编译包。前往 [GitHub Releases](https://github.com/clice-io/catter/releases) 页面，下载对应平台的压缩包，解压后将 `catter` 可执行文件所在目录加入 `PATH` 即可。

预编译包当前支持的平台：

| 平台    | 架构       |
| ------- | ---------- |
| Windows | x64、arm64 |
| Linux   | x64        |
| macOS   | arm64      |

::: info
Catter 目前仍在积极开发中，请以 Releases 页面为准获取最新版本。如果您的平台没有预编译包，可以参阅[从源码构建](../dev/build)自行编译。
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
