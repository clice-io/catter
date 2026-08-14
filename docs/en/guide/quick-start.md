# Quick Start

## Prerequisites

- A C++ project with any build system (Make, Ninja, CMake, Meson, etc.)

## Installation

Catter is distributed as prebuilt packages on [GitHub Releases](https://github.com/clice-io/catter/releases). Download the archive for your platform, extract it, and add the directory containing the `catter` executable to your `PATH`.

Prebuilt packages are currently available for:

| Platform | Architecture |
|----------|--------------|
| Windows  | x64 |
| Linux    | x64 |
| macOS    | arm64 |

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

After building, `api/` is a complete npm package: `api/package.json`, `api/dist/` (runtime JS modules), and `api/types/` (TypeScript declarations). Build it from the repository root:

```bash
pixi run -e dev npm-install
pixi run -e dev build-js
```

Then add `api/` to your script project as a local dependency (npm / pnpm / yarn):

```bash
npm install --save-dev /path/to/catter/api
```

Or declare it in `package.json`:

```json
{
  "devDependencies": {
    "catter": "file:../path/to/catter/api"
  }
}
```

This lets your script project import `catter/*` modules with full IDE autocompletion and type checking.

::: info
The `catter` npm package is not published to the registry yet; use the local dependency approach above until then. Once published, you can simply `npm install --save-dev catter`.
:::
