# Scripting System Overview

Catter embeds [QuickJS-ng](https://github.com/nicklausw/quickjs-ng) (v0.11.0) as its JavaScript engine. Scripts are written in JavaScript (or TypeScript with type checking via the `catter` npm package). The runtime is not Node.js -- it is a lightweight embedded engine with a custom API surface.

## Script Lifecycle

1. User runs: `catter <script> [script-args] -- <build-command>`
2. Catter loads and evaluates the script.
3. Script registers service callbacks via `service.register()`.
4. `onStart(config)` is called -- script can inspect and modify the build configuration.
5. Build execution begins (if `config.execute` is true).
6. For each intercepted command: `onCommand(ctx)` is called -- script decides the action.
7. After each command execution: `onExecution(ctx)` is called with the result.
8. Build completes: `onFinish(result)` is called with the process exit code, stdout, and stderr.

## Script Types

**Built-in scripts** are selected with the `script::` prefix:

| Script | Description |
|--------|-------------|
| `script::cdb` | Generate a `compile_commands.json` compilation database |
| `script::cmd-tree` | Display the captured build command DAG as an ASCII tree |
| `script::target-tree` | Display the build target dependency tree |

**Custom scripts** can be any `.js` file path:

```bash
catter ./my-script.js -- cmake --build build
```

## Type Support for Development

Install the `catter` npm package to get TypeScript type definitions when writing custom scripts:

```bash
npm install --save-dev catter
```

Then in your script:

```js
// @ts-check
import { service, fs, io } from "catter";
```

This provides IDE autocompletion and type checking for the entire catter scripting API.

## Minimal Example

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
    // Return nothing = let it execute normally
  },
  onFinish(result) {
    io.println(`Build finished with code ${result.code}`);
  }
});
```

## Available Modules

The `catter` package exposes several namespaced modules:

| Module | Purpose |
|--------|---------|
| `service` | Service lifecycle registration and callbacks |
| `cmd` | Command analysis (compiler, archiver) |
| `option` | Compiler option table parsing and rewriting |
| `fs` | File system operations and path utilities |
| `io` | Standard output, colored printing, file streams |
| `os` | Platform and architecture detection |
| `http` | HTTP client for GET, POST, and other requests |
| `time` | Wall-clock and monotonic timestamps, duration helpers |
| `debug` | Assertion helpers |
| `cli` | Declarative argument parsing for script options |
| `data` | Data structures (FlatTree for DAGs) |
| `scripts` | Pre-built script factories (`scripts.cdb()`, etc.) |

Import them individually:

```js
import { service, cmd, option, fs, io, os, http, time, debug, cli } from "catter";
```
