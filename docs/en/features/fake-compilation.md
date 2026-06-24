# Fake Compilation

> [!WARNING]
> This feature is planned but not yet implemented.

## Concept

Instead of forwarding compilation to the real compiler, catter generates fake placeholder `.o` files. This lets the build system complete its full run -- including link steps -- without actual compilation.

The result:

- **A complete CDB in a fraction of the time.** No real compilation work is performed, so the build finishes as fast as the build system can schedule it.
- **Full linker command capture.** Because placeholder object files exist on disk, linker invocations proceed normally and can be intercepted.

## Smart Dependency Analysis

Not all commands can be faked. Code generators -- such as LLVM TableGen -- must still be built genuinely, because they produce headers that other compilations depend on.

Catter will analyze dependencies to distinguish between:

- **Regular compilation** -- Can be faked. The output `.o` file is only consumed by the linker.
- **Code generators** -- Must be built. Their output (generated headers, source files) is needed as input by other compilation steps.

This achieves a "minimal build": only build what is strictly necessary for correct CDB generation and header production, and fake everything else.
