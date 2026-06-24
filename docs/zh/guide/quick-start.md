# 快速上手

## 前置条件

- [pixi](https://pixi.sh) 环境管理工具
- 使用任意构建系统（Make、Ninja、CMake、Meson 等）的 C++ 项目

## 安装

Catter 通过 pixi 分发，使用以下命令安装：

```bash
pixi global install catter
```

::: info
Catter 目前处于 v0.1.0 阶段，仍在积极开发中，安装方式可能在后续版本中发生变化。
:::

## 生成编译数据库

通过 catter 运行你的构建命令，即可捕获所有编译器调用：

```bash
catter script::cdb -o compile_commands.json -- make
```

这会在当前目录生成 `compile_commands.json` 文件，可直接用于任何语言服务器或静态分析工具。

## 命令格式

```
catter [选项] <脚本> [脚本参数] -- <构建命令>
```

以上面的 CDB 示例为例：

| 部分 | 含义 |
|------|------|
| `script::cdb` | 使用内置的 CDB 生成脚本（`script::` 前缀表示内置脚本） |
| `-o compile_commands.json` | 脚本特定选项：输出文件路径 |
| `--` | catter/脚本参数与构建命令之间的分隔符 |
| `make` | 实际要拦截的构建命令 |

## 更多示例

### 命令树可视化

将捕获的构建命令 DAG 以 ASCII 树形式展示：

```bash
catter script::cmd-tree -- make
```

### 配合 CMake 使用

```bash
catter script::cdb -o compile_commands.json -- cmake --build build
```

### 自定义脚本

使用自己编写的 JavaScript 脚本替代内置脚本：

```bash
catter ./my-script.js -- cmake --build build
```

## 脚本开发的 IDE 支持

安装 catter npm 包以获取 TypeScript 类型定义，方便编写自定义脚本时使用 IDE 自动补全：

```bash
npm install --save-dev catter
```

这将提供 catter 脚本 API 的 IDE 自动补全和类型检查支持。
