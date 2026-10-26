# Quick Start

## Prerequisites

- A C++ project with any build system (Make, Ninja, CMake, Meson, etc.)

## Installation

Catter is distributed as prebuilt packages on [GitHub Releases](https://github.com/clice-io/catter/releases). Download the archive for your platform, extract it, and add the directory containing the `catter` executable to your `PATH`.

On Linux, extract it to a directory whose path has no space and no colon, and on macOS no colon: the variable that loads catter's hook into the build (`LD_PRELOAD`, `DYLD_INSERT_LIBRARIES`) splits paths there, and catter refuses to run a build from such a directory.

Prebuilt packages are currently available for:

| Platform | Architecture |
| -------- | ------------ |
| Windows  | x64, arm64   |
| Linux    | x64          |
| macOS    | arm64        |

::: info
Catter is under active development; always use the latest package from the Releases page. If there is no prebuilt package for your platform, see [Building from Source](../dev/build).
:::

## Generate a Compilation Database

Run your build command through catter to capture all compiler invocations:

```bash
catter compile-commands -o compile_commands.json -- make
```

This produces a `compile_commands.json` file in the current directory, ready for use with any language server or static analysis tool.

## Command Format

```
catter <command> [options] -- <build-command>
```

Breaking down the CDB example:

| Part                       | Meaning                                                         |
| -------------------------- | --------------------------------------------------------------- |
| `compile-commands`         | The command: generate a CDB                                     |
| `-o compile_commands.json` | An option of the command: the output file path                  |
| `--`                       | Separator between the command's arguments and the build command |
| `make`                     | The actual build command to intercept                           |

`catter --help` lists the commands, and `catter <command> --help` shows the options of one.

## More Examples

### Command tree visualization

View the captured build command DAG as an ASCII tree:

```bash
catter tree -- make
```

### Using with CMake

```bash
catter compile-commands -o compile_commands.json -- cmake --build build
```

### Your own handler

Hand the commands to a program of your own, a [handler](../scripting/handlers.md):

```bash
catter run --handler "python my_handler.py" -- cmake --build build
```
