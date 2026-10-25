# Compilation Database

The compilation database (CDB) is catter's primary feature. It intercepts compiler invocations during a build and generates a standard `compile_commands.json` file, compatible with clang tooling, language servers, and IDEs.

Command: `compile-commands`

## Usage

```bash
catter compile-commands [options] -- <build-command>
```

## Options

| Option                       | Description                                                                             |
| ---------------------------- | --------------------------------------------------------------------------------------- |
| `-o, --output <path>`        | Write the compilation database to this path. Defaults to `build/compile_commands.json`. |
| `--replace`                  | Ignore existing database entries and replace the output file.                           |
| `--save-on-failure`          | Save collected entries even when the build exits with a non-zero code.                  |
| `--abort-on-command-failure` | Abort when any captured command exits with a non-zero code.                             |
| `--abort-on-capture-error`   | Abort when catter reports a command capture error.                                      |
| `-q, --quiet`                | Suppress informational output.                                                          |
| `-v, --verbose`              | Print detailed command analysis and generated entries.                                  |

A relative output path is from the `-C` directory. `compile-commands` also takes the options of every command that runs a build: `-C`, `--from` and the handlers' (see [Configuration](../guide/configuration.md)).

## Behavior

By default, catter **merges** with an existing `compile_commands.json` if one is found at the output path. New entries for the same source file replace old ones, so you can incrementally rebuild without losing entries from previous runs.

Internally, catter:

1. Intercepts each compiler invocation during the build.
2. Analyzes the command using `CompilerAnalysis` to identify source files, output files, and compiler flags.
3. Builds a `FlatTree` (DAG) of command relationships to track input-to-output edges.
4. On completion, traverses the tree to leaf source files and generates one CDB entry per source file.

## CDB Entry Format

The output follows the standard clang JSON compilation database format:

```json
[
  {
    "directory": "/path/to/build",
    "file": "/path/to/source.cpp",
    "arguments": ["clang++", "-std=c++20", "-c", "source.cpp", "-o", "source.o"],
    "output": "source.o"
  }
]
```

## Supported Compilers

- **C/C++**: GCC, Clang

Compiler wrappers such as `ccache`, `distcc`, and `sccache` are recognized and handled transparently. Catter can also identify other compilers (Flang, ifort, NVCC, etc.) but `CompilerAnalysis` currently only generates CDB entries for GCC and Clang commands.

## Examples

```bash
# Basic usage with make
catter compile-commands -- make -j8

# Specify output path
catter compile-commands -o build/compile_commands.json -- ninja

# With CMake
catter compile-commands -- cmake --build build

# With any build system
catter compile-commands -- ./build.sh
```
