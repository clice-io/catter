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
| `--abort-on-capture-error`   | catter 报告命令捕获错误或无法读取的响应文件时中止。            |
| `-q, --quiet`                | 不输出提示信息。                                               |
| `-v, --verbose`              | 打印命令分析的细节和生成的条目。                               |

相对的输出路径从 `-C` 指定的目录算起。`compile-commands` 也接受所有运行构建的命令共有的选项：`-C`、`--capture`、`--from` 和 handler 的选项（见[配置参考](../guide/configuration.md)）。

## 行为

默认情况下，如果输出路径已存在 `compile_commands.json`，catter 会与之**合并**。源文件和输出都相同的新条目会替换旧条目，因此可以增量构建而不丢失之前的记录。已有文件如果 catter 无法当作数据库读取，catter 会在构建运行之前停下：用 `--replace` 写一个新的数据库覆盖它。数据库要么整个写入，要么完全不写，因此中途被打断的运行会留下原来的数据库。

编译命令的响应文件（`@file`）在命令启动时、运行之前读取，因为 MSBuild 等构建工具会在命令结束后删除它们（`eslogger` 在命令启动之后才得知它，构建在此之前已删除的文件就读不到了：catter 会像报告无法捕获的命令那样，报告无法读取的响应文件）：条目里是这些文件中的参数，按编译器的方式切分，MSVC 的 `cl`、`clang-cl` 和 cl 模式下的 clang 用 Windows 的规则。

每条编译器命令对它的每个源文件、以及由这个源文件写出的每个文件，各生成一条条目，条目带有这条命令的目录、参数，以及命令里对源文件的写法；没有写出任何文件的源文件（例如 `-fsyntax-only`）得到一条不带 `output` 的条目。预处理（`-E`、`-M`、cl 的 `/scanDependencies`）只为构建中没有其他命令编译的源文件生成条目，且不带 `output`。构建中某条编译命令写出的文件（例如 `-S` 得到的 `.s`）如果又被构建编译，它本身没有条目：由它的源文件的条目代表它。构建中 gcc 或 clang 的某次预处理写出的文件如果又被编译成目标文件，这次编译得到的是该预处理所代表的那次编译的条目，文件是预处理的源文件：参数取预处理的参数，去掉 `-E`、`-P`、`-fdirectives-only`、`-fpch-preprocess`、`-c` 及其输出，再加上 `-c -o` 和目标文件。预处理写出的文件如果在构建结束前被构建删掉，这次预处理也得到这样的条目，只是不带目标文件：xmake 的构建缓存就是把每个源文件预处理成这样一个文件，用它查找目标文件，缓存中没有时才编译这个文件。探测编译没有条目：编译的文件在构建结束前被构建删掉（configure 脚本的探测就是这样），或者是 xmake 放在临时目录下自己目录中的文件，或者是 CMake 在 catter 下配置时对编译器的检查：CMake 在它的 `CMakeFiles` 目录中运行这些检查，或者编译它自带模块中的文件。

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

- GCC、Clang、`clang-cl` 和 MSVC 的 `cl`，名字前可以有目标前缀、后面可以有版本号（`x86_64-linux-gnu-gcc-13`、MacPorts 的 `clang-mp-18`），源文件可以是 C、C++（包括模块）、Objective-C、Objective-C++、汇编和 CUDA，也可以是 `-x` 指定的任何语言。

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
