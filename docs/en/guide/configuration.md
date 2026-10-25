# Configuration

## catter CLI

```
catter [options] [<built-in> [built-in options]] -- <build-command>
```

A run takes a built-in, one or more [handlers](../scripting/handlers.md), or both.

### Options

| Option                        | Description                                                                                                                                                     | Default           |
| ----------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------- |
| `-m, --mode <mode>`           | Runtime mode: `inject` runs the build command and intercepts its processes; `replay` plays a recorded build, whose events file stands in for the build command. | `inject`          |
| `-d, --dir <path>`            | Working directory for the target process.                                                                                                                       | Current directory |
| `--handler <command>`         | A handler for catter to start; may be given more than once. See [Handlers](../scripting/handlers.md).                                                           |                   |
| `--connect <host:port>`       | A handler listening at the address; may be given more than once. See [Handlers](../scripting/handlers.md).                                                      |                   |
| `--allow-remote`              | Let `--connect` reach a handler on another machine.                                                                                                             |                   |
| `--handler-timeout <seconds>` | How long catter waits for a handler to answer, and to exit once the session is over; `0` waits as long as it takes.                                             | `60`              |
| `-h, --help`                  | Show help message.                                                                                                                                              |                   |

### Data directory

catter keeps its logs and the socket of each running session in `$HOME/.catter` (Linux, macOS) or `%LOCALAPPDATA%\catter` (Windows). The `CATTER_HOME` environment variable, when set, names another directory, by an absolute path.

### Built-ins

A built-in is named by itself, before its options:

```bash
catter <name> [built-in options] -- <build-command>
```

The built-ins:

| Name          | Description                                            |
| ------------- | ------------------------------------------------------ |
| `cdb`         | Generate `compile_commands.json`                       |
| `cmd-tree`    | Display the build command tree                         |
| `target-tree` | Display the build target tree                          |
| `inspect`     | Show how catter analyzes a command, without running it |

## catter-proxy CLI

::: warning
`catter-proxy` is an internal component. It is invoked automatically by catter and is not intended for direct use.
:::

```
catter-proxy [options] -- <command>
```

| Option         | Description                       |
| -------------- | --------------------------------- |
| `-p <id>`      | Parent process ID for IPC session |
| `<executable>` | Resolved executable path          |

## Environment Variables

These variables are set automatically by catter at runtime. They are documented here for reference; you should not need to set them manually.

### Unix (Linux / macOS)

| Variable                        | Purpose                                |
| ------------------------------- | -------------------------------------- |
| `__key_catter_proxy_path_v1`    | Path to the `catter-proxy` executable  |
| `__key_catter_command_id_v1`    | IPC command identifier                 |
| `LD_PRELOAD` (Linux)            | Injects the catter hook shared library |
| `DYLD_INSERT_LIBRARIES` (macOS) | Injects the catter hook shared library |

### Windows

| Variable            | Purpose                               |
| ------------------- | ------------------------------------- |
| `CATTER_IPC_ID`     | IPC session identifier                |
| `CATTER_PROXY_PATH` | Path to the `catter-proxy` executable |
