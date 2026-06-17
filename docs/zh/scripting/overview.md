# 脚本系统概述

Catter 内嵌了 [QuickJS-ng](https://github.com/nicklausw/quickjs-ng) (v0.11.0) 作为 JavaScript 引擎。脚本使用 JavaScript 编写（也可通过 `catter` npm 包获得 TypeScript 类型检查支持）。该运行时并非 Node.js，而是一个轻量的嵌入式引擎，提供专属的 API 接口。

## 脚本生命周期

1. 用户运行：`catter <script> [script-args] -- <build-command>`
2. Catter 加载并执行脚本。
3. 脚本通过 `service.register()` 注册服务回调。
4. 调用 `onStart(config)` —— 脚本可以查看和修改构建配置。
5. 开始执行构建（如果 `config.execute` 为 true）。
6. 每拦截到一个命令，调用 `onCommand(ctx)` —— 脚本决定如何处理该命令。
7. 每个命令执行完毕后，调用 `onExecution(ctx)` 并传入执行结果。
8. 构建完成时，调用 `onFinish(result)` 并传入进程退出码、stdout 和 stderr。

## 脚本类型

**内置脚本**使用 `script::` 前缀选择：

| 脚本 | 描述 |
|------|------|
| `script::cdb` | 生成 `compile_commands.json` 编译数据库 |
| `script::cmd-tree` | 以 ASCII 树形式展示捕获的构建命令 DAG |
| `script::target-tree` | 展示构建目标的依赖树 |

**自定义脚本**可以是任意 `.js` 文件路径：

```bash
catter ./my-script.js -- cmake --build build
```

## 开发时的类型支持

安装 `catter` npm 包以获取 TypeScript 类型定义：

```bash
npm install --save-dev catter
```

然后在脚本中：

```js
// @ts-check
import { service, fs, io } from "catter";
```

这样可以在 IDE 中获得完整的自动补全和类型检查支持。

## 最小示例

```js
import { service, io } from "catter";

service.register({
  onStart(config) {
    io.println("Build starting...");
    return config;
  },
  onCommand(ctx) {
    if (ctx.capture.success) {
      io.println(`Command: ${ctx.capture.data.argv[0]}`);
    }
    // 不返回任何内容 = 让命令正常执行
  },
  onFinish(result) {
    io.println(`Build finished with code ${result.code}`);
  }
});
```

## 可用模块

`catter` 包提供以下命名空间模块：

| 模块 | 用途 |
|------|------|
| `service` | 服务生命周期注册和回调 |
| `cmd` | 命令分析（编译器、归档工具） |
| `option` | 编译器选项表解析和重写 |
| `fs` | 文件系统操作和路径工具 |
| `io` | 标准输出、彩色打印、文件流 |
| `os` | 平台和架构检测 |
| `http` | HTTP 客户端，支持 GET、POST 等请求 |
| `time` | 时间戳和时间间隔工具 |
| `debug` | 断言辅助函数 |
| `cli` | 脚本参数声明式解析 |
| `data` | 数据结构（用于 DAG 的 FlatTree） |
| `scripts` | 预构建脚本工厂（`scripts.cdb()` 等） |

按需导入：

```js
import { service, cmd, option, fs, io, os, http, time, debug, cli } from "catter";
```
