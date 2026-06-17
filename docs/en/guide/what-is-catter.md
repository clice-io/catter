# What is Catter?

Catter is a build process interception tool. It hooks into child process creation during a build, captures compilation and linker commands, and lets you process them with JavaScript scripts powered by an embedded QuickJS runtime.

## Motivation

Catter was created for [clice](https://github.com/clice-io/clice), a new C++ language server. Language servers depend on a [Compilation Database (CDB)](https://clang.llvm.org/docs/JSONCompilationDatabase.html) to understand how each source file is compiled, but obtaining one is often painful:

- **Limited build system support** -- CMake only generates CDB when using Makefile or Ninja backends. Many other popular C++ build systems have no native CDB support at all.
- **Inaccessible build directories** -- When C++ code is built as part of a Python package (or similar), the build directory is managed externally and the CDB file is out of reach.
- **Code generators require a full build** -- Projects like LLVM use tools such as `tablegen` that generate header files at build time. These files are referenced by `-I` flags in the compilation commands, so the language server cannot resolve includes until a full build has run -- even if you only want to read the code.

No existing cross-platform tool solves all of these problems. That is why we created catter.

## How It Works

Catter's approach is similar to [Bear](https://github.com/rizsotto/Bear) and [scan-build](https://github.com/rizsotto/scan-build). It intercepts child process creation using platform-specific hooks (`LD_PRELOAD` on Linux, `DYLD_INSERT_LIBRARIES` on macOS, DLL injection on Windows). Every intercepted command is sent to a decision-making daemon that runs user-provided JavaScript scripts via QuickJS, determining what to do with each command.

## Core Capabilities

### Compilation Database Generation

The primary use case. Catter captures compiler invocations across any build system and generates a standard `compile_commands.json`. No build system plugins or special configuration needed.

### Linker Analysis

By capturing linker commands, catter can reconstruct the dependency graph between build targets. This is essential for C++20 modules support: the C++ standard says a program may have at most one module with a given name, and "program" is defined by targets (libraries, executables). This target information is missing from the CDB standard, but catter can infer it from linker commands.

### Fake Compilation (Planned)

Instead of forwarding compilation to the real compiler, catter will be able to generate placeholder object files. This lets the build system run to completion without actually compiling, producing a complete CDB in a fraction of the time. Code generators (like `tablegen`) are still built normally -- catter analyzes dependencies to determine the minimal set of tools that must be genuinely compiled.

### Build Profiling (Planned)

Capture process timing, durations, and parent-child relationships during a build. The data can be rendered visually in a browser, letting you analyze parallelism and identify bottlenecks.

### Custom Script Patching

With the embedded QuickJS engine, users can write JavaScript scripts to dynamically modify, redirect, or inject logic into build commands -- without changing the original build files.

## Architecture at a Glance

Catter is a three-part system:

- **HOOK** -- Platform-specific library injected into processes to intercept child process creation.
- **PROXY** (`catter-proxy`) -- Manages hook injection and acts as a compiler wrapper. Relays intercepted commands to the decision daemon.
- **DECISION** (`catter`) -- The main daemon. Runs the JS runtime, receives intercepted commands from the proxy, and decides how to handle each one.

For a detailed walkthrough of the architecture and IPC flow, see the [design documentation](../design/).
