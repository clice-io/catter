# Command Analysis

The `cmd` module provides structured analysis of compiler and archiver command lines. It identifies the phase, artifact type, input files, output files, and dependency edges for intercepted build commands.

## CompilerAnalysis

Analyzes compiler command lines (GCC, Clang) to extract structured information.

```js
import { cmd, io, service } from "catter";

service.onCommand((ctx) => {
  if (!ctx.capture.success) return;

  const argv = ctx.capture.data.argv;
  const analysis = cmd.CompilerAnalysis.analyze(argv);

  if (analysis) {
    io.println(`Phase: ${analysis.phase}`);
    io.println(`Artifact: ${analysis.artifact}`);
    io.println(`Compiler: ${analysis.compiler}`);
    io.println(`Sources: ${analysis.sourceInputs().join(", ")}`);
    io.println(`Outputs: ${analysis.outputs().join(", ")}`);
  }
});
```

### Static Methods

| Method | Description |
|--------|-------------|
| `CompilerAnalysis.supports(argv)` | Check if command looks like a compiler invocation |
| `CompilerAnalysis.analyze(argv)` | Full analysis, returns `undefined` for non-compiler commands |
| `CompilerAnalysis.from(analysis)` | Narrow a generic `Analysis` back to `CompilerAnalysis` |

### Instance Properties

| Property | Type | Description |
|----------|------|-------------|
| `compiler` | `"clang" \| "gcc"` | Recognized compiler executable |
| `phase` | `CompilerPhase` | Pipeline phase (see below) |
| `artifact` | `CompilerArtifact` | Output artifact type (see below) |
| `style` | `"gnu" \| "cl"` | Driver option syntax style |
| `type` | `CompilerType \| undefined` | Legacy classification (for compatibility) |

### Instance Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `inputs()` | `string[]` | All input file paths |
| `sourceInputs()` | `string[]` | Only source file paths |
| `inputEntries()` | `CompilerInput[]` | Inputs with metadata: `{path, kind, index}` |
| `outputs()` | `string[]` | Produced output file paths |
| `edges()` | `Edge[]` | Dependency edges (input-to-output mappings) |

Each `CompilerInput` contains:

- `path` -- file path
- `kind` -- `"source"` or `"link"`
- `index` -- position in the original argv

Each `Edge` contains:

- `output` -- the output file path
- `inputs` -- array of input file paths that produce this output

For compile commands, edges pair each object file with its own source file. For link commands, all inputs feed into each output.

### Supported Phases

| Phase | Value |
|-------|-------|
| `CompilerPhase.Preprocess` | `"preprocess"` |
| `CompilerPhase.SyntaxOnly` | `"syntax-only"` |
| `CompilerPhase.Compile` | `"compile"` |
| `CompilerPhase.Link` | `"link"` |
| `CompilerPhase.Archive` | `"archive"` |
| `CompilerPhase.RelocatableLink` | `"relocatable-link"` |
| `CompilerPhase.DeviceLink` | `"device-link"` |

### Supported Artifact Types

| Artifact | Value |
|----------|-------|
| `CompilerArtifact.None` | `"none"` |
| `CompilerArtifact.Stdout` | `"stdout"` |
| `CompilerArtifact.Object` | `"object"` |
| `CompilerArtifact.Executable` | `"exe"` |
| `CompilerArtifact.SharedLibrary` | `"shared"` |
| `CompilerArtifact.StaticLibrary` | `"static-lib"` |
| `CompilerArtifact.Assembly` | `"asm"` |
| `CompilerArtifact.LlvmIR` | `"llvm-ir"` |
| `CompilerArtifact.LlvmBitcode` | `"llvm-bc"` |
| `CompilerArtifact.Pch` | `"pch"` |
| `CompilerArtifact.Pcm` | `"pcm"` |
| `CompilerArtifact.Ptx` | `"ptx"` |
| `CompilerArtifact.Cubin` | `"cubin"` |
| `CompilerArtifact.Fatbin` | `"fatbin"` |

## ArchiverAnalysis

Analyzes archive commands (`ar`, `llvm-ar`, `gcc-ar`).

```js
import { cmd } from "catter";

const argv = ["llvm-ar", "rcs", "libfoo.a", "foo.o", "bar.o"];
const analysis = cmd.ArchiverAnalysis.analyze(argv);

if (analysis) {
  io.println(`Operation: ${analysis.operation}`);  // "r"
  io.println(`Archive: ${analysis.archive}`);       // "libfoo.a"
  io.println(`Members: ${analysis.members}`);       // ["foo.o", "bar.o"]
  io.println(`Thin: ${analysis.thin}`);             // false
}
```

### Instance Properties

| Property | Type | Description |
|----------|------|-------------|
| `operation` | `ArchiverOperation` | Archive operation (`"r"`, `"x"`, `"t"`, etc.) |
| `modifiers` | `string[]` | Modifier letters (e.g., `["c", "s"]`) |
| `thin` | `boolean` | Whether thin-archive mode was requested |
| `archive` | `string \| undefined` | Archive file path |
| `members` | `string[]` | Member file paths |

### Supported Operations

The analyzer currently models these operations:

| Operation | Value | Description |
|-----------|-------|-------------|
| `ArchiverOperation.Print` | `"p"` | Print members |
| `ArchiverOperation.QuickAppend` | `"q"` | Quick append |
| `ArchiverOperation.ReplaceOrInsert` | `"r"` | Replace or insert members |
| `ArchiverOperation.Table` | `"t"` | List contents |

Other `ar` operations (`d`, `m`, `s`, `x`) are recognized by the enum but `ArchiverAnalysis.analyze()` returns `undefined` for them.

## Registry

Analyzers are organized in a `Registry`. The default registry includes `CompilerAnalysis` and `ArchiverAnalysis`.

### Using the Default Registry

```js
import { cmd } from "catter";

// Analyze with all built-in analyzers
const analysis = cmd.analyze(["clang", "-c", "main.c"]);

// Check if any analyzer handles the command
const canHandle = cmd.canHandle(["clang", "-c", "main.c"]);
```

### Custom Registry

```js
import { cmd } from "catter";

const registry = new cmd.Registry();
registry.register(cmd.CompilerAnalysis);
registry.register(cmd.ArchiverAnalysis);

const analysis = registry.analyze(["gcc", "-c", "main.c"]);
```

### Adding Custom Analyzers

Implement the `Analyzer` interface with `key`, `supports()`, and `analyze()`:

```js
import { cmd } from "catter";

class MyToolAnalysis extends cmd.Analysis {
  static key = "my-tool";

  static supports(argv) {
    return argv[0]?.endsWith("my-tool") ?? false;
  }

  static analyze(argv) {
    if (!MyToolAnalysis.supports(argv)) return undefined;
    return new MyToolAnalysis(argv);
  }

  constructor(argv) {
    const inputs = [argv[1]];
    const outputs = [argv[2]];
    super("my-tool", inputs, outputs);
  }
}

// Register globally
cmd.register(MyToolAnalysis);

// Or use a separate registry
const registry = new cmd.Registry();
registry.register(MyToolAnalysis);
```

### Narrowing Analysis Results

When you get a generic `Analysis` from the registry, use the static `from()` method to narrow it:

```js
const analysis = cmd.analyze(argv);
const compiler = cmd.CompilerAnalysis.from(analysis);
const archiver = cmd.ArchiverAnalysis.from(analysis);

if (compiler) {
  io.println(`Compiler phase: ${compiler.phase}`);
} else if (archiver) {
  io.println(`Archiver operation: ${archiver.operation}`);
}
```
