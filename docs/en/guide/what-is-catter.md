# What is Catter?

Catter is a build process interception tool. It hooks into child process creation during a build, captures compilation and linker commands, and lets you process them with its built-ins or with handlers: programs of your own, written in any language.

## Motivation

Catter was created for [clice](https://github.com/clice-io/clice), a new C++ language server. Language servers depend on a [Compilation Database (CDB)](https://clang.llvm.org/docs/JSONCompilationDatabase.html) to understand how each source file is compiled, but obtaining one is often painful:

- **Limited build system support** -- CMake only generates CDB when using Makefile or Ninja backends. Many other popular C++ build systems have no native CDB support at all.
- **Inaccessible build directories** -- When C++ code is built as part of a Python package (or similar), the build directory is managed externally and the CDB file is out of reach.
- **Code generators require a full build** -- Projects like LLVM use tools such as `tablegen` that generate header files at build time. These files are referenced by `-I` flags in the compilation commands, so the language server cannot resolve includes until a full build has run -- even if you only want to read the code.

No existing cross-platform tool solves all of these problems. That is why we created catter.

## How It Works

Catter's approach is similar to [Bear](https://github.com/rizsotto/Bear) and [scan-build](https://github.com/rizsotto/scan-build). It intercepts child process creation using platform-specific hooks (`LD_PRELOAD` on Linux, `DYLD_INSERT_LIBRARIES` on macOS, DLL injection on Windows). Every intercepted command is sent to a decision-making daemon, which asks the session's handlers what to do with it.

## Core Capabilities

### Compilation Database Generation

The primary use case. Catter captures compiler invocations across any build system and generates a standard `compile_commands.json`. No build system plugins or special configuration needed.

### Linker Analysis

By capturing linker commands, catter can reconstruct the dependency graph between build targets. This is essential for C++20 modules support: the C++ standard says a program may have at most one module with a given name, and "program" is defined by targets (libraries, executables). This target information is missing from the CDB standard, but catter can infer it from linker commands.

### Fake Compilation (Planned)

Instead of forwarding compilation to the real compiler, catter will be able to generate placeholder object files. This lets the build system run to completion without actually compiling, producing a complete CDB in a fraction of the time. Code generators (like `tablegen`) are still built normally -- catter analyzes dependencies to determine the minimal set of tools that must be genuinely compiled.

### Build Profiling

Catter records when each command of a build started, how long it ran and which command started it, and writes them as a trace you open in Perfetto or `chrome://tracing`, to see how parallel the build really is and where its time goes.

### Custom Handlers

A handler, a program of your own that catter speaks to over a small protocol, can observe every command, or decide on each one: run it as it is, run another command in its place, drop it, or abort the build -- without changing the original build files. SDKs for Python and TypeScript speak the protocol for you.

## Architecture at a Glance

Catter is organized around **capture backends**: a capture backend is how catter gets a build's commands. The default, `hook`, intercepts child process creation through platform hooks (Linux `LD_PRELOAD`, macOS `DYLD_INSERT_LIBRARIES`, Windows DLL injection); every captured command is handed to the daemon, which passes it to the session's handlers.

A handler is told when the session starts and how its commands are captured, of each command before it runs (or, under a backend that only observes the build, once it has started) and once it has run, and when the build ends. A handler that decides runs each command as it is, runs it modified, drops it, or aborts the build; one that does not only observes.

For details on the architecture concepts, capture backends, and the IPC flow, see the [design documentation](../design/architecture) and [Capture backends](../design/capture-backends).
