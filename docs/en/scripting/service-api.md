# Service API Reference

The service API is the core of catter scripting. It lets you register callbacks that respond to build lifecycle events: start, command interception, execution results, and finish.

## Registration

Register a service object with one or more callbacks:

```js
import { service } from "catter";

service.register({
  onStart(config) { /* ... */ },
  onCommand(ctx) { /* ... */ },
  onExecution(ctx) { /* ... */ },
  onFinish(result) { /* ... */ }
});
```

All callbacks are optional. You can also use convenience methods to register individual callbacks:

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

Called before build execution. Receives the runtime configuration object. Return the (possibly modified) config to apply changes, or return nothing to accept it as-is.

**CatterConfig fields:**

| Field | Type | Description |
|-------|------|-------------|
| `scriptPath` | `string` | Path to the script being executed |
| `scriptArgs` | `string[]` | Arguments passed to the script (before `--`) |
| `buildSystemCommand` | `string[]` | The build command (after `--`) |
| `buildSystemCommandCwd` | `string` | Working directory for the build |
| `runtime` | `CatterRuntime` | Runtime capabilities (see below) |
| `options` | `object` | Options like log level, stdio mode |
| `execute` | `boolean` | Set to `false` to prevent build execution |

**CatterRuntime fields:**

| Field | Type | Description |
|-------|------|-------------|
| `type` | `"inject" \| "eslogger" \| "env"` | Interception mode |
| `supportActions` | `ActionType[]` | Which actions the runtime supports |
| `supportParentId` | `boolean` | Whether parent-child process tracking is available |

**Example -- skip execution and just inspect config:**

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

Called for each intercepted process creation. The callback receives a `CommandContext` object and can decide what happens to the command.

**CommandContext fields:**

| Field | Type | Description |
|-------|------|-------------|
| `id` | `number` | Unique command ID |
| `capture` | `CommandCaptureResult` | Captured command data (see below) |
| `action` | `Action` | Current action (default: `{type: "skip"}`) |
| `stopped` | `boolean` | Whether propagation has been stopped |

The `capture` field is a tagged result. On success:

- `capture.success` is `true`
- `capture.data.argv` -- full argument array
- `capture.data.cwd` -- working directory
- `capture.data.env` -- environment variables
- `capture.data.parent` -- parent process ID (if available)

**Action methods on ctx:**

| Method | Effect |
|--------|--------|
| `ctx.skip()` | Let the command execute normally but ignore it in catter |
| `ctx.drop()` | Prevent the command from executing |
| `ctx.modify(data)` | Execute a modified command instead |
| `ctx.ignoreDescendants()` | Don't intercept child processes of this command |
| `ctx.stopPropagation()` | Stop calling remaining service handlers |

If the callback returns nothing (undefined), the command proceeds normally. You can also return an action object directly:

```js
service.onCommand((ctx) => {
  if (!ctx.capture.success) {
    return; // skip capture errors
  }

  const argv = ctx.capture.data.argv;
  if (argv[0].endsWith("ccache")) {
    ctx.ignoreDescendants();
    return;
  }

  // Drop all link commands
  if (argv.includes("-shared")) {
    ctx.drop();
  }
});
```

## onExecution

```
onExecution(ctx: ExecutionContext) => void
```

Called after each command finishes execution.

**ExecutionContext fields:**

| Field | Type | Description |
|-------|------|-------------|
| `id` | `number` | Same command ID from `onCommand` |
| `result` | `ProcessResult` | Process result (see below) |
| `stopped` | `boolean` | Whether propagation has been stopped |

**ProcessResult fields:**

| Field | Type | Description |
|-------|------|-------------|
| `code` | `number` | Exit code |
| `stdout` | `string` | Standard output (if captured) |
| `stderr` | `string` | Standard error (if captured) |

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

Called when the entire build process completes.

```js
service.onFinish((result) => {
  if (result.code === 0) {
    io.coloredPrintln("Build succeeded.", "green");
  } else {
    io.coloredPrintln(`Build failed with code ${result.code}.`, "red");
  }
});
```

## Multiple Services

You can call `service.register()` multiple times. Services are called in registration order. Use `ctx.stopPropagation()` to prevent later services from seeing a command.

```js
// First service: filter out ccache wrappers
service.register({
  onCommand(ctx) {
    if (ctx.capture.success && ctx.capture.data.argv[0].endsWith("ccache")) {
      ctx.ignoreDescendants();
    }
  }
});

// Second service: analyze remaining commands
service.register({
  onCommand(ctx) {
    // This won't see commands whose propagation was stopped
  }
});
```

## Service Composition

The `service` module also provides `pipeline()` and `parallel()` for composing services:

```js
import { service } from "catter";

// Sequential pipeline -- services run in order
const combined = service.pipeline(serviceA, serviceB);

// Parallel execution -- services run concurrently
const parallel = service.parallel(serviceA, serviceB);

service.register(combined);
```

With `service.parallel()`, at most one service may set an action for any given command. If multiple services attempt to set actions, an error is thrown.

## Factory Helper

Use `service.create()` to construct a service object with typed context-style callbacks:

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
