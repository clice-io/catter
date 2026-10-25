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
catter cdb -o compile_commands.json -- make
```

This produces a `compile_commands.json` file in the current directory, ready for use with any language server or static analysis tool.

## Command Format

```
catter [options] <built-in> [built-in options] -- <build-command>
```

Breaking down the CDB example:

| Part                       | Meaning                                                                           |
| -------------------------- | --------------------------------------------------------------------------------- |
| `cdb`                      | Use the built-in that generates a CDB                                             |
| `-o compile_commands.json` | An option of the built-in: the output file path                                   |
| `--`                       | Separator between the arguments of catter and the built-in, and the build command |
| `make`                     | The actual build command to intercept                                             |

## More Examples

### Command tree visualization

View the captured build command DAG as an ASCII tree:

```bash
catter cmd-tree -- make
```

### Using with CMake

```bash
catter cdb -o compile_commands.json -- cmake --build build
```

### Your own handler

Hand the commands to a program of your own, a [handler](../scripting/handlers.md):

```bash
catter --handler "python my_handler.py" -- cmake --build build
```
