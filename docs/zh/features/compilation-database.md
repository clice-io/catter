# 编译数据库

编译数据库 (CDB) 是 catter 的核心功能。它在构建过程中拦截编译器调用，生成标准的 `compile_commands.json` 文件，兼容 clang 工具链、语言服务器和各类 IDE。

命令：`compile-commands`

## 用法

```bash
catter compile-commands [options] -- <build-command>
```

## 选项

| 选项                         | 说明                                                           |
| ---------------------------- | -------------------------------------------------------------- |
| `-o, --output <path>`        | 把编译数据库写到该路径。默认为 `build/compile_commands.json`。 |
| `--replace`                  | 忽略已有数据库中的条目，替换输出文件。                         |
| `--save-on-failure`          | 即使构建以非零退出码结束，也保存已收集的条目。                 |
| `--abort-on-command-failure` | 任一被捕获的命令以非零退出码结束时中止。                       |
| `--abort-on-capture-error`   | catter 报告命令捕获错误时中止。                                |
| `-q, --quiet`                | 不输出提示信息。                                               |
| `-v, --verbose`              | 打印命令分析的细节和生成的条目。                               |

相对的输出路径从 `-C` 指定的目录算起。`compile-commands` 也接受所有运行构建的命令共有的选项：`-C`、`--from` 和 handler 的选项（见[配置参考](../guide/configuration.md)）。

## 行为

默认情况下，如果输出路径已存在 `compile_commands.json`，catter 会与之**合并**。相同源文件的新条目会替换旧条目，因此可以增量构建而不丢失之前的记录。

编译命令的响应文件（`@file`）在命令启动时、运行之前读取，因为 MSBuild 等构建工具会在命令结束后删除它们：条目里是这些文件中的参数，按编译器的方式切分，MSVC 的 `cl`、`clang-cl` 和 cl 模式下的 clang 用 Windows 的规则。

内部流程：

1. 在构建过程中拦截每一次编译器调用。
2. 使用 `CompilerAnalysis` 分析命令，识别源文件、输出文件和编译器标志。
3. 构建 `FlatTree`（有向无环图）来追踪输入到输出的边关系。
4. 构建完成后，遍历树到叶节点源文件，为每个源文件生成一条 CDB 条目。

## CDB 条目格式

输出遵循标准的 clang JSON 编译数据库格式：

```json
[
  {
    "directory": "/path/to/build",
    "file": "/path/to/source.cpp",
    "arguments": ["clang++", "-std=c++20", "-c", "source.cpp", "-o", "source.o"],
    "output": "source.o"
  }
]
```

## 支持的编译器

- **C/C++**：GCC、Clang

编译器包装器如 `ccache`、`distcc`、`sccache` 可被正确识别和透明处理。Catter 也能识别其他编译器（Flang、ifort、NVCC 等），但 `CompilerAnalysis` 目前仅为 GCC 和 Clang 命令生成 CDB 条目。

## 示例

```bash
# Basic usage with make
catter compile-commands -- make -j8

# Specify output path
catter compile-commands -o build/compile_commands.json -- ninja

# With CMake
catter compile-commands -- cmake --build build

# With any build system
catter compile-commands -- ./build.sh
```
