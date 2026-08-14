# 快速上手

## 前置条件

- 使用任意构建系统（Make、Ninja、CMake、Meson 等）的 C++ 项目

## 安装

Catter 通过 GitHub Releases 分发预编译包。前往 [GitHub Releases](https://github.com/clice-io/catter/releases) 页面，下载对应平台的压缩包，解压后将 `catter` 可执行文件所在目录加入 `PATH` 即可。

预编译包当前支持的平台：

| 平台 | 架构 |
|------|------|
| Windows | x64 |
| Linux | x64 |
| macOS | arm64 |

::: info
Catter 目前仍在积极开发中，请以 Releases 页面为准获取最新版本。如果您的平台没有预编译包，可以参阅[从源码构建](../dev/build)自行编译。
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

`api/` 构建后会生成一个完整的 npm 包，包含 `api/package.json`、`api/dist/`（运行时 JS 模块）和 `api/types/`（TypeScript 类型声明）。先在仓库根目录构建它：

```bash
pixi run -e dev npm-install
pixi run -e dev build-js
```

然后将 `api/` 作为本地依赖直接添加到你的脚本项目中（npm / pnpm / yarn 均可）：

```bash
npm install --save-dev /path/to/catter/api
```

或在 `package.json` 中声明：

```json
{
  "devDependencies": {
    "catter": "file:../path/to/catter/api"
  }
}
```

这样脚本项目即可通过 `catter/*` 导入模块，并获得完整的 IDE 自动补全和类型检查。

::: info
`catter` 尚未发布到 npm registry，在此之前请使用上面的本地依赖方式；发布后可直接 `npm install --save-dev catter`。
:::
