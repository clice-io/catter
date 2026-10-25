# Quick Start

## Prerequisites

- A C++ project with any build system (Make, Ninja, CMake, Meson, etc.)

## Installation

Catter is distributed as prebuilt packages on [GitHub Releases](https://github.com/clice-io/catter/releases). Download the archive for your platform, extract it, and add the directory containing the `catter` executable to your `PATH`.

Prebuilt packages are currently available for:

| Platform | Architecture |
| -------- | ------------ |
| Windows  | x64          |
| Linux    | x64          |
| macOS    | arm64        |

::: info
Catter is under active development; always use the latest package from the Releases page. If there is no prebuilt package for your platform, see [Building from Source](../dev/build).
:::

## Generate a Compilation Database

Run your build command through catter to capture all compiler invocations:

```bash
catter script::cdb -o compile_commands.json -- make
```

This produces a `compile_commands.json` file in the current directory, ready for use with any language server or static analysis tool.

## Command Format

```
catter [options] <script> [script-args] -- <build-command>
```

Breaking down the CDB example:

| Part                       | Meaning                                                                             |
| -------------------------- | ----------------------------------------------------------------------------------- |
| `script::cdb`              | Use the built-in CDB generation script (`script::` prefix selects built-in scripts) |
| `-o compile_commands.json` | Script-specific option: output file path                                            |
| `--`                       | Separator between catter/script arguments and the build command                     |
| `make`                     | The actual build command to intercept                                               |

## More Examples

### Command tree visualization

View the captured build command DAG as an ASCII tree:

```bash
catter script::cmd-tree -- make
```

### Using with CMake

```bash
catter script::cdb -o compile_commands.json -- cmake --build build
```

### Your own handler

Hand the commands to a program of your own, a [handler](../scripting/handlers.md):

```bash
catter --handler "python my_handler.py" -- cmake --build build
```
