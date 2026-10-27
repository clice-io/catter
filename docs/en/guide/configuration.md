# Configuration

## catter CLI

```
catter <command> [options] -- <build-command>
```

Every run names a command, and its options come after it. `catter --help` lists the commands, and `catter <command> --help` shows the options of one. Without a command, catter lists the commands and exits with an error.

### Commands

| Command            | Description                                                                                                                               |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------------------------- |
| `compile-commands` | Generate a `compile_commands.json` file from captured compiler commands. See [Compilation Database](../features/compilation-database.md). |
| `tree`             | Render captured commands as a command tree. See [Command Tree](../features/command-tree.md).                                              |
| `targets`          | Render captured build targets as a flat target forest. See [Target Tree](../features/target-tree.md).                                     |
| `explain`          | Print how catter analyzes the command after `--`, without running it. See [Explain](../features/explain.md).                              |
| `run`              | Run the build with handlers alone, which decide on and observe its commands. See [Handlers](../scripting/handlers.md).                    |

`compile-commands`, `tree` and `targets` each run a built-in on the build, together with the [handlers](../scripting/handlers.md) the command line names, if any. `run` runs the handlers alone, and needs at least one.

### Options

`compile-commands`, `tree`, `targets` and `run` take these options besides their own:

| Option                        | Description                                                                                                                   | Default           |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------- | ----------------- |
| `-C, --directory <path>`      | The directory the command runs in.                                                                                            | Current directory |
| `--from <file>`               | Replay the build recorded in this file instead of running one.                                                                |                   |
| `--handler <command>`         | Start a handler, which decides or observes the build's commands over its stdin and stdout; may be given more than once.       |                   |
| `--connect <host:port>`       | Connect to a handler listening at the address, on this machine unless `--allow-remote` is given; may be given more than once. |                   |
| `--allow-remote`              | Let `--connect` reach a handler on another machine.                                                                           |                   |
| `--handler-timeout <seconds>` | How long catter waits for a handler to answer, and to exit once the session is over; `0` waits as long as it takes.           | `60`              |

`--from` takes the place of the build command after `--`: a run gives one or the other. A relative path is from the `-C` directory.

`explain` takes `-C` too, and the command to analyze after `--`.

### Exit status

catter ends as the build did: with the build's exit code, or, when a signal ended the build, by the same signal. A replayed build ends with the exit code its file records. When catter itself fails, as when the command line is wrong, a handler aborts the build or the compilation database cannot be written, catter prints why and exits with 1.

Stopping catter stops the build: a SIGTERM, SIGINT or SIGHUP sent to catter goes on to the build command, which ends as it would if it were sent to it, and catter then ends as the build did. A build command ends with catter when catter is killed.

### Data directory

catter keeps its logs and the socket of each running session in `$HOME/.catter` (Linux, macOS) or `%LOCALAPPDATA%\catter` (Windows). The `CATTER_HOME` environment variable, when set, names another directory, by an absolute path. On Linux and macOS, what the build's processes log goes to the directory of their session, whatever `HOME` or `CATTER_HOME` a build step gives them.

## catter-proxy CLI

::: warning
`catter-proxy` is an internal component of catter on Linux and macOS. It is invoked automatically by catter and is not intended for direct use.
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

catter sets none: it gives the hook in each process of the build its session directly, and the build's environment stays as the build made it.
