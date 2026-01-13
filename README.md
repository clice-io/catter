# Catter: A New Build Process Interception Tool

![C++ Standard](https://img.shields.io/badge/C++-23-blue.svg)
[![GitHub license](https://img.shields.io/github/license/clice-io/catter)](https://github.com/clice-io/catter/blob/main/LICENSE)

> [!IMPORTANT]
> This project is under active development. Stay tuned for future updates!

## Motivation

We are developing a new C++ language server, [clice](https://github.com/clice-io/clice). For it to work correctly, users often need to provide a [Compilation Database (CDB)](https://clang.llvm.org/docs/JSONCompilationDatabase.html). This is a JSON file that records the compilation commands for all source files.

Unfortunately, not all C++ build systems support the generation of CDB files. For example, CMake only supports this feature when using `Makefile` or `Ninja` as the build backend. Furthermore, C++ has many other popular build systems, and many of them do not natively support CDB generation, often requiring special workarounds. In some situations, you might not even be able to access a CDB even if it is generated; for instance, when a Python package builds C++ code, you often have no control over the build directory and thus cannot retrieve the corresponding CDB file.

Moreover, even with a CDB, a language server might still not function correctly. A classic example is LLVM. Why? Because LLVM uses its own compiled `tablegen` tool for code generation. The generated files are placed in a specific directory, which is then added to the compilation flags with `-I`. This means that if you only run `cmake configure`, these generated files will be missing, causing the language server to fail. You must run `cmake build` to generate them. Unfortunately, it's difficult to configure LLVM to *only* generate these necessary headers for a minimal build, especially if your goal is just to read the code. A full build of LLVM takes a very long time. It would be ideal if we could build only the minimal required components.

Currently, there is no cross-platform tool that completely solves all these issues. This is why we decided to create `catter`.

## Core Features

`catter`'s underlying principle is similar to [Bear](https://github.com/rizsotto/Bear) or [scan-build](https://github.com/rizsotto/scan-build). It works by using hooks to intercept any child process creation (including compilation tasks) initiated by the target process. It also integrates QuickJS, allowing you to write JavaScript scripts to process these events—for example, to log or modify compilation arguments, analyze build times, and so on.

Although its initial motivation was to record compilation commands to generate a CDB, we quickly realized it could be used for much more:

*   **Analyze Linker Commands to Infer Target Information**: By capturing linker commands, `catter` can analyze the dependency graph between targets. This provides richer target information, which is essential for language servers to properly support C++20 modules. The C++ standard stipulates that a program can have at most one module with the same name, and it is the target information that distinguishes different programs (e.g., libraries vs. executables). This crucial information is missing from the current CDB standard, with no prospects for improvement in the near future. With `catter`, we can solve this problem once and for all, across all build systems.

*   **Perform "Fake" Compilations**: Instead of forwarding compilation tasks to the actual compiler, `catter` can generate fake placeholder object files. This allows the build system to proceed with its tasks without performing a full, time-consuming compilation. This way, you can obtain a complete CDB without waiting for the entire build process to finish. **Note**: If the build involves code generators, those tools must still be genuinely built. `catter` can automatically analyze dependencies to ensure that only these essential tools are compiled, achieving a true minimal build.

*   **Profile the Build Process**: Due to issues in the build system's design or poorly written build scripts, the actual degree of build parallelism can be very low. We can capture process information (start times, durations, parent-child relationships) and render it visually, allowing users to inspect the build's parallelism and performance bottlenecks in real-time in a browser.

*   **Patch the Build Process with Custom Scripts**: Thanks to the embedded QuickJS engine, users can write their own scripts to patch any command during the build process. This gives you the power to dynamically modify arguments, redirect commands, or inject custom logic into your build without altering the original build files.