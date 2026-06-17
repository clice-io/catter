# Quick Start

## Prerequisites

- [pixi](https://pixi.sh) for environment management
- A C++ project with any build system (Make, Ninja, CMake, Meson, etc.)

## Installation

Catter uses pixi for distribution. Install it with:

```bash
pixi global install catter
```

::: info
Catter is at v0.1.0 and under active development. The installation method may change in future releases.
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

| Part | Meaning |
|------|---------|
| `script::cdb` | Use the built-in CDB generation script (`script::` prefix selects built-in scripts) |
| `-o compile_commands.json` | Script-specific option: output file path |
| `--` | Separator between catter/script arguments and the build command |
| `make` | The actual build command to intercept |

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

### Custom scripts

Use your own JavaScript script instead of a built-in one:

```bash
catter ./my-script.js -- cmake --build build
```

## IDE Support for Script Development

Install the catter npm package to get TypeScript type definitions when writing custom scripts:

```bash
npm install --save-dev catter
```

This provides IDE autocompletion and type checking for the catter scripting API.
