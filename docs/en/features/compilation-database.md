# Compilation Database

The compilation database (CDB) is catter's primary feature. It intercepts compiler invocations during a build and generates a standard `compile_commands.json` file, compatible with clang tooling, language servers, and IDEs.

Built-in script: `script::cdb`

## Usage

```bash
catter script::cdb [options] -- <build-command>
```

## Script Options

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output path for `compile_commands.json`. Defaults to `build/compile_commands.json`. |
| `--abort-on-command-failure` | Abort the entire build if any intercepted command fails. |
| `--save-on-failure` | Save partial CDB even if the build fails. |

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
catter script::cdb -- make -j8

# Specify output path
catter script::cdb -o build/compile_commands.json -- ninja

# With CMake
catter script::cdb -- cmake --build build

# With any build system
catter script::cdb -- ./build.sh
```
