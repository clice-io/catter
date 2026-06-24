# Compiler Option Parsing

The `option` module provides full option table parsing for major C/C++ toolchains, derived from LLVM's option definitions. It can parse, inspect, rewrite, and convert compiler options across toolchains.

## Supported Option Tables

| Table | Toolchain |
|-------|-----------|
| `"clang"` | Clang/GCC driver options |
| `"nvcc"` | NVIDIA CUDA compiler |
| `"lld-elf"` | LLD linker (ELF/Linux) |
| `"lld-coff"` | LLD linker (COFF/Windows) |
| `"lld-macho"` | LLD linker (Mach-O/macOS) |
| `"lld-mingw"` | LLD linker (MinGW) |
| `"lld-wasm"` | LLD linker (WebAssembly) |
| `"llvm-lib"` | LLVM library manager |
| `"llvm-dlltool"` | LLVM DLL tool |

## Collecting Parsed Options

Parse all options from an argument array:

```js
import { option } from "catter";

const args = ["-std=c++20", "-O2", "-c", "main.cc", "-o", "main.o"];
const items = option.collect("clang", args);

if (typeof items === "string") {
  throw new Error(`Parse error: ${items}`);
}

for (const item of items) {
  const meta = option.info("clang", item);
  io.println(`${meta.prefixedKey} => ${item.values.join(", ")}`);
}
```

## Streaming Parse

Parse with a callback instead of collecting:

```js
import { option } from "catter";

option.parse("clang", args, (result) => {
  if (typeof result === "string") {
    io.println(`Error: ${result}`);
    return false; // stop parsing
  }
  io.println(`Option: ${result.key}, values: ${result.values}`);
  return true; // continue
});
```

## Option Inspection

Get metadata about a parsed option:

```js
const items = option.collect("clang", ["-std=c++20"]);
if (Array.isArray(items)) {
  const info = option.info("clang", items[0]);
  io.println(info.prefixedKey); // "-std="
  io.println(info.kind);       // OptionKindClass.JoinedClass
  io.println(info.help);       // help text from LLVM option table
}
```

## Rendering Options

Convert a parsed option item back to a command-line string:

```js
const str = option.stringify("clang", item);
io.println(str); // e.g., "-std=c++20"
```

## Alias Resolution

Convert an aliased option to its canonical form:

```js
const items = option.collect("nvcc", ["-ofoo.o"]);
if (Array.isArray(items)) {
  option.convertToUnalias("nvcc", items[0]);
  io.println(items[0].key); // "--output-file"
}
```

Note: `convertToUnalias()` mutates the item in place and returns it for convenience.

## Rewriting Arguments

Replace or remove options in an argument array:

```js
import { option } from "catter";

const rewritten = option.replace("clang", ["-Iold", "-O0", "main.cc"], (result) => {
  if (typeof result === "string") {
    throw new Error(result);
  }

  // Replace include path
  if (result.key === "-I") {
    return { ...result, values: ["new-include"] };
  }

  // Remove optimization flag (return a string to replace the span)
  if (result.key === "-O") {
    return "-O2";
  }

  // Return undefined to keep unchanged
});

io.println(rewritten); // "-Inew-include -O2 main.cc"
```

The callback can return:

| Return value | Effect |
|--------------|--------|
| `undefined` | Keep the original text |
| `OptionItem` | Replace with the rendered item |
| `string` | Replace with the literal string |
| `string[]` | Replace with the joined array |
| `boolean` | `true` to continue, `false` to stop |

## Cross-Table Conversion

Convert arguments from one option table to another, keeping only options that are valid in the target table:

```js
import { option } from "catter";

const nvccArgs = ["--gpu-architecture=sm_70", "-std=c++17", "-O2"];
const clangArgs = option.table2table("nvcc", "clang", nvccArgs);

if (typeof clangArgs === "string") {
  throw new Error(clangArgs);
}

io.println(clangArgs.join(" ")); // options valid in both tables
```

The function parses with the source table, splits args into per-option spans, then re-parses each span with the target table. Spans that parse as `UnknownClass` or match excluded IDs are dropped.

## OptionItem Structure

Each parsed option is represented as an `OptionItem`:

| Field | Type | Description |
|-------|------|-------------|
| `key` | `string` | Option key (e.g., `"-std="`, `"-c"`, `"-I"`) |
| `values` | `string[]` | Option values array |
| `id` | `number` | Numeric option ID from the table |
| `unalias` | `number \| undefined` | Canonical option ID if this is an alias |
| `index` | `number` | Position of this option in the original args |

## OptionInfo Structure

Metadata returned by `option.info()`:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `number` | Numeric option ID |
| `prefixedKey` | `string` | Full option string (e.g., `"-std="`) |
| `kind` | `OptionKindClass` | Option kind (see below) |
| `group` | `number` | Group ID |
| `alias` | `number` | Alias target ID |
| `aliasArgs` | `string[]` | Arguments appended when resolving alias |
| `flags` | `number` | Option flags |
| `visibility` | `number` | Platform visibility bitmask |
| `param` | `number` | Parameter count for multi-arg options |
| `help` | `string` | Help text from LLVM table |
| `meta_var` | `string` | Metavar label for help text |

## Option Kinds

The `OptionKindClass` enum describes how an option consumes arguments:

| Kind | Description |
|------|-------------|
| `FlagClass` | No value (e.g., `-c`, `-v`) |
| `JoinedClass` | Value joined to key (e.g., `-std=c++20`) |
| `SeparateClass` | Value in next argument (e.g., `-o main.o`) |
| `JoinedOrSeparateClass` | Either joined or separate (e.g., `-Ipath` or `-I path`) |
| `CommaJoinedClass` | Comma-separated values joined to key |
| `RemainingArgsClass` | Consumes all remaining arguments |
| `RemainingArgsJoinedClass` | Key + remaining args |
| `MultiArgClass` | Fixed number of separate value arguments |
| `ValuesClass` | Multiple separate values |
| `InputClass` | Positional input (source file) |
| `UnknownClass` | Unrecognized option |
| `GroupClass` | Option group (not a real option) |

## Visibility Filtering

Options can be filtered by platform visibility when parsing. This is useful for handling platform-specific options like MSVC-style `/TC`:

```js
import { option } from "catter";

// Parse with default visibility (all options visible)
const items = option.collect("clang", args);

// Parse with explicit visibility mask to filter platform-specific options
const clItems = option.collect("clang", args, ClangVisibility.CLOption);
```

Each option table exports its own visibility constants (e.g., `ClangVisibility.DefaultVis`, `ClangVisibility.CLOption`).
