# 编译数据库

编译数据库 (CDB) 是 catter 的核心功能。它在构建过程中拦截编译器调用，生成标准的 `compile_commands.json` 文件，兼容 clang 工具链、语言服务器和各类 IDE。

内置脚本：`script::cdb`

## 用法

```bash
catter script::cdb [options] -- <build-command>
```

## 脚本选项

| 选项 | 说明 |
|------|------|
| `-o, --output <path>` | `compile_commands.json` 的输出路径。默认为 `build/compile_commands.json`。 |
| `--abort-on-command-failure` | 任一被拦截的命令失败时，中止整个构建。 |
| `--save-on-failure` | 即使构建失败，也保存已收集的部分 CDB。 |

## 行为

默认情况下，如果输出路径已存在 `compile_commands.json`，catter 会与之**合并**。相同源文件的新条目会替换旧条目，因此可以增量构建而不丢失之前的记录。

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
# 配合 make 基本使用
catter script::cdb -- make -j8

# 指定输出路径
catter script::cdb -o build/compile_commands.json -- ninja

# 配合 CMake
catter script::cdb -- cmake --build build

# 配合任意构建系统
catter script::cdb -- ./build.sh
```
