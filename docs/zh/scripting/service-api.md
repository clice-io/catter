# Service API 参考

Service API 是 catter 脚本系统的核心。它允许你注册回调函数来响应构建生命周期事件：启动、命令拦截、执行结果和完成。

## 注册

注册一个包含一个或多个回调的服务对象：

```js
import { service } from "catter";

service.register({
  onStart(config) { /* ... */ },
  onCommand(ctx) { /* ... */ },
  onExecution(ctx) { /* ... */ },
  onFinish(result) { /* ... */ }
});
```

所有回调都是可选的。也可以使用便捷方法单独注册某个回调：

```js
service.onStart((config) => { /* ... */ });
service.onCommand((ctx) => { /* ... */ });
service.onExecution((ctx) => { /* ... */ });
service.onFinish((result) => { /* ... */ });
```

## onStart

```
onStart(config: CatterConfig) => CatterConfig | void
```

在构建执行前调用。接收运行时配置对象。返回（可能已修改的）配置以应用更改，或不返回任何内容表示接受当前配置。

**CatterConfig 字段：**

| 字段 | 类型 | 描述 |
|------|------|------|
| `scriptPath` | `string` | 正在执行的脚本路径 |
| `scriptArgs` | `string[]` | 传递给脚本的参数（`--` 之前的部分） |
| `buildSystemCommand` | `string[]` | 构建命令（`--` 之后的部分） |
| `buildSystemCommandCwd` | `string` | 构建命令的工作目录 |
| `runtime` | `CatterRuntime` | 运行时能力（见下文） |
| `options` | `object` | 日志级别、stdio 模式等选项 |
| `execute` | `boolean` | 设为 `false` 可阻止构建执行 |

**CatterRuntime 字段：**

| 字段 | 类型 | 描述 |
|------|------|------|
| `type` | `"inject" \| "eslogger" \| "env"` | 拦截模式 |
| `supportActions` | `ActionType[]` | 运行时支持的动作类型 |
| `supportParentId` | `boolean` | 是否支持父子进程追踪 |

**示例 -- 跳过执行，仅查看配置：**

```js
service.onStart((config) => {
  io.println(`Build command: ${config.buildSystemCommand.join(" ")}`);
  io.println(`Script args: ${config.scriptArgs.join(" ")}`);
  config.execute = false;
  return config;
});
```

## onCommand

```
onCommand(ctx: CommandContext) => Action | void
```

每拦截到一个进程创建时调用。回调接收一个 `CommandContext` 对象，可以决定如何处理该命令。

**CommandContext 字段：**

| 字段 | 类型 | 描述 |
|------|------|------|
| `id` | `number` | 命令唯一 ID |
| `capture` | `CommandCaptureResult` | 捕获的命令数据（见下文） |
| `action` | `Action` | 当前动作（默认为 `{type: "skip"}`） |
| `stopped` | `boolean` | 传播是否已被停止 |

`capture` 字段是一个带标签的结果类型。成功时：

- `capture.success` 为 `true`
- `capture.data.argv` -- 完整参数数组
- `capture.data.cwd` -- 工作目录
- `capture.data.env` -- 环境变量
- `capture.data.parent` -- 父进程 ID（如果可用）

**ctx 上的动作方法：**

| 方法 | 效果 |
|------|------|
| `ctx.skip()` | 让命令正常执行，但在 catter 中忽略它 |
| `ctx.drop()` | 阻止命令执行 |
| `ctx.abort()` | 中止整个构建 |
| `ctx.modify(data)` | 执行修改后的命令 |
| `ctx.ignoreDescendants()` | 不拦截该命令的子进程 |
| `ctx.stopPropagation()` | 停止调用后续的服务处理器 |

如果回调不返回任何内容（undefined），命令将正常执行。也可以直接返回一个 action 对象：

```js
service.onCommand((ctx) => {
  if (!ctx.capture.success) {
    return; // 跳过捕获错误
  }

  const argv = ctx.capture.data.argv;
  if (argv[0].endsWith("ccache")) {
    ctx.ignoreDescendants();
    return;
  }

  // 丢弃所有链接命令
  if (argv.includes("-shared")) {
    ctx.drop();
  }
});
```

## onExecution

```
onExecution(ctx: ExecutionContext) => void
```

每个命令执行完毕后调用。

**ExecutionContext 字段：**

| 字段 | 类型 | 描述 |
|------|------|------|
| `id` | `number` | 与 `onCommand` 中相同的命令 ID |
| `result` | `ProcessResult` | 进程执行结果（见下文） |
| `stopped` | `boolean` | 传播是否已被停止 |

**ProcessResult 字段：**

| 字段 | 类型 | 描述 |
|------|------|------|
| `code` | `number` | 退出码 |
| `stdout` | `string` | 标准输出（如果已捕获） |
| `stderr` | `string` | 标准错误（如果已捕获） |

```js
service.onExecution((ctx) => {
  if (ctx.result.code !== 0) {
    io.coloredPrintln(`Command ${ctx.id} failed with code ${ctx.result.code}`, "red");
  }
});
```

## onFinish

```
onFinish(result: ProcessResult) => void
```

整个构建过程完成时调用。

```js
service.onFinish((result) => {
  if (result.code === 0) {
    io.coloredPrintln("Build succeeded.", "green");
  } else {
    io.coloredPrintln(`Build failed with code ${result.code}.`, "red");
  }
});
```

## 多服务

可以多次调用 `service.register()`。服务按注册顺序依次调用。使用 `ctx.stopPropagation()` 可以阻止后续服务接收到某个命令。

```js
// 第一个服务：过滤 ccache 包装器
service.register({
  onCommand(ctx) {
    if (ctx.capture.success && ctx.capture.data.argv[0].endsWith("ccache")) {
      ctx.ignoreDescendants();
    }
  }
});

// 第二个服务：分析剩余命令
service.register({
  onCommand(ctx) {
    // 不会看到传播已被停止的命令
  }
});
```

## 服务组合

`service` 模块还提供了 `pipeline()` 和 `parallel()` 用于组合服务：

```js
import { service } from "catter";

// 顺序管道 -- 服务按顺序执行
const combined = service.pipeline(serviceA, serviceB);

// 并行执行 -- 服务同时运行
const parallel = service.parallel(serviceA, serviceB);

service.register(combined);
```

使用 `service.parallel()` 时，对于任何给定命令，最多只能有一个服务设置动作。如果多个服务同时尝试设置动作，将会抛出错误。

## 工厂辅助函数

使用 `service.create()` 构造带类型上下文回调的服务对象：

```js
import { service } from "catter";

const myService = service.create({
  onStart(config) { return config; },
  onCommand(ctx) { /* ... */ },
  onExecution(ctx) { /* ... */ },
  onFinish(result) { /* ... */ },
});

service.register(myService);
```
