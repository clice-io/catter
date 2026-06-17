# Configuration

## catter CLI

```
catter [options] <script> [script-args] -- <build-command>
```

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `-m, --mode <mode>` | Runtime mode. Controls how catter intercepts processes. | `inject` |
| `-d, --dir <path>` | Working directory for the target process. | Current directory |
| `--stdio-mode <mode>` | How to handle child process stdio. See below. | `inherit` |
| `-h, --help` | Show help message. | |

### `--stdio-mode`

Controls what happens with the intercepted process's standard output and error streams:

- **`inherit`** -- Real-time passthrough. Build output appears in your terminal as it normally would.
- **`capture`** -- Buffer stdout and stderr. The captured output is made available to the script's `onFinish` callback instead of being printed immediately.

### Script Specification

**Built-in scripts** use the `script::` prefix:

```bash
catter script::<name> [script-args] -- <build-command>
```

Available built-in scripts:

| Name | Description |
|------|-------------|
| `script::cdb` | Generate `compile_commands.json` |
| `script::cmd-tree` | Display the build command tree |
| `script::target-tree` | Display the build target tree |

**Custom scripts** use a file path:

```bash
catter ./path/to/script.js [script-args] -- <build-command>
```

## catter-proxy CLI

::: warning
`catter-proxy` is an internal component. It is invoked automatically by catter and is not intended for direct use.
:::

```
catter-proxy [options] -- <command>
```

| Option | Description |
|--------|-------------|
| `-p <id>` | Parent process ID for IPC session |
| `<executable>` | Resolved executable path |

## Environment Variables

These variables are set automatically by catter at runtime. They are documented here for reference; you should not need to set them manually.

### Unix (Linux / macOS)

| Variable | Purpose |
|----------|---------|
| `__key_catter_proxy_path_v1` | Path to the `catter-proxy` executable |
| `__key_catter_command_id_v1` | IPC command identifier |
| `LD_PRELOAD` (Linux) | Injects the catter hook shared library |
| `DYLD_INSERT_LIBRARIES` (macOS) | Injects the catter hook shared library |

### Windows

| Variable | Purpose |
|----------|---------|
| `CATTER_IPC_ID` | IPC session identifier |
| `CATTER_PROXY_PATH` | Path to the `catter-proxy` executable |
