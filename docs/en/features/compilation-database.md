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

A relative output path is from the `-C` directory. `compile-commands` also takes the options of every command that runs a build: `-C`, `--capture`, `--from` and the handlers' (see [Configuration](../guide/configuration.md)).

## Behavior

By default, catter **merges** with an existing `compile_commands.json` if one is found at the output path. New entries for the same source file and output replace old ones, so you can incrementally rebuild without losing entries from previous runs. An existing file catter cannot read as a database stops it before the build runs: `--replace` writes a new one over it. The database is written whole or not at all, so a run that is cut short leaves the old one.

A compile's response files (`@file`) are read when the command starts, before it runs, as build tools such as MSBuild delete them once it has: its entries hold the arguments the files hold, split as the compiler splits them, with Windows' rules for MSVC's `cl`, `clang-cl` and clang in cl mode.

Each compiler command gets an entry for each of its sources and each file it writes from that source, with the command's directory, its arguments and the source as it names it; a source it writes nothing from, as with `-fsyntax-only`, gets an entry without `output`. A preprocess (`-E`, `-M`, cl's `/scanDependencies`) gets one only for a source no other command of the build compiles, and without `output`. A file a compile of the build writes, such as the `.s` of `-S`, gets no entry of its own when the build compiles it too: the entry of its source stands for it. A compile into an object of a file a preprocess of gcc or clang wrote gets the entry of the compile the preprocess stands for, of its source: the preprocess's arguments without `-E`, `-P`, `-fdirectives-only`, `-fpch-preprocess`, `-c` and its output, then `-c -o` and the object. So does, without an object, a preprocess whose file the build removes before it ends: xmake's build cache preprocesses each source to such a file to look its object up, and compiles that file only when the cache does not have it. A probe gets no entry: a compile of a file the build removes before it ends, as a configure script's, one of xmake's in its directory of the temporary directory, or, when CMake configures under catter, one of CMake's checks of the compiler: a compile CMake runs in its `CMakeFiles` directory, or of a file of its own modules.

## CDB Entry Format

The output follows the standard clang JSON compilation database format:

```json
[
  {
    "directory": "/path/to/build",
    "file": "../src/source.cpp",
    "arguments": ["clang++", "-std=c++20", "-c", "../src/source.cpp", "-o", "source.o"],
    "output": "/path/to/build/source.o"
  }
]
```

## Supported Compilers

- GCC, Clang, `clang-cl` and MSVC's `cl`, their names perhaps after a target and before a version (`x86_64-linux-gnu-gcc-13`, MacPorts' `clang-mp-18`), for sources in C, C++ (modules too), Objective-C and Objective-C++, assembly and CUDA, or in any language `-x` names.

Compiler wrappers such as `ccache`, `distcc`, and `sccache` are recognized and handled transparently. catter recognizes nvcc, but does not read its arguments yet: its commands get no entries.

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
