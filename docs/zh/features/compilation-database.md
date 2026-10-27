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

默认情况下，如果输出路径已存在 `compile_commands.json`，catter 会与之**合并**。源文件和输出都相同的新条目会替换旧条目，因此可以增量构建而不丢失之前的记录。已有文件如果 catter 无法当作数据库读取，catter 会在构建运行之前停下：用 `--replace` 写一个新的数据库覆盖它。数据库要么整个写入，要么完全不写，因此中途被打断的运行会留下原来的数据库。

编译命令的响应文件（`@file`）在命令启动时、运行之前读取，因为 MSBuild 等构建工具会在命令结束后删除它们：条目里是这些文件中的参数，按编译器的方式切分，MSVC 的 `cl`、`clang-cl` 和 cl 模式下的 clang 用 Windows 的规则。

每条编译器命令对它的每个源文件、以及由这个源文件写出的每个文件，各生成一条条目，条目带有这条命令的目录、参数，以及命令里对源文件的写法；没有写出任何文件的源文件（例如 `-fsyntax-only`）得到一条不带 `output` 的条目。预处理（`-E`、`-M`）只为构建中没有其他命令编译的源文件生成条目，且不带 `output`。构建中某条编译命令写出的文件（例如 `-S` 得到的 `.s`）如果又被构建编译，它本身没有条目：由它的源文件的条目代表它。

## CDB 条目格式

输出遵循标准的 clang JSON 编译数据库格式：

```json
[
  {
    "directory": "/path/to/build",
    "file": "../src/source.cpp",
    "arguments": ["clang++", "-std=c++20", "-c", "../src/source.cpp", "-o", "source.o"],
    "output": "/path/to/build/source.o"
  }
]
```

## 支持的编译器

- GCC、Clang、`clang-cl` 和 MSVC 的 `cl`，源文件可以是 C、C++（包括模块）、Objective-C、Objective-C++、汇编和 CUDA，也可以是 `-x` 指定的任何语言。

编译器包装器如 `ccache`、`distcc`、`sccache` 可被正确识别和透明处理。catter 能识别 nvcc，但还不读取它的参数：它的命令没有条目。

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
