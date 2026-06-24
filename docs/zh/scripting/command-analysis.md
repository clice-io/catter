# 命令分析

`cmd` 模块提供编译器和归档工具命令行的结构化分析。它能识别编译阶段、产物类型、输入文件、输出文件以及依赖边。

## CompilerAnalysis

分析编译器命令行（GCC、Clang），提取结构化信息。

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

### 静态方法

| 方法 | 描述 |
|------|------|
| `CompilerAnalysis.supports(argv)` | 检查命令是否看起来像编译器调用 |
| `CompilerAnalysis.analyze(argv)` | 完整分析，非编译器命令返回 `undefined` |
| `CompilerAnalysis.from(analysis)` | 将通用 `Analysis` 收窄为 `CompilerAnalysis` |

### 实例属性

| 属性 | 类型 | 描述 |
|------|------|------|
| `compiler` | `"clang" \| "gcc"` | 识别到的编译器 |
| `phase` | `CompilerPhase` | 编译阶段（见下文） |
| `artifact` | `CompilerArtifact` | 输出产物类型（见下文） |
| `style` | `"gnu" \| "cl"` | 驱动选项的语法风格 |
| `type` | `CompilerType \| undefined` | 旧版兼容分类 |

### 实例方法

| 方法 | 返回值 | 描述 |
|------|--------|------|
| `inputs()` | `string[]` | 所有输入文件路径 |
| `sourceInputs()` | `string[]` | 仅源文件路径 |
| `inputEntries()` | `CompilerInput[]` | 带元数据的输入：`{path, kind, index}` |
| `outputs()` | `string[]` | 生成的输出文件路径 |
| `edges()` | `Edge[]` | 依赖边（输入到输出的映射） |

每个 `CompilerInput` 包含：

- `path` -- 文件路径
- `kind` -- `"source"` 或 `"link"`
- `index` -- 在原始 argv 中的位置

每个 `Edge` 包含：

- `output` -- 输出文件路径
- `inputs` -- 产生该输出的输入文件路径数组

对于编译命令，边会将每个目标文件与其对应的源文件配对。对于链接命令，所有输入都指向每个输出。

### 支持的阶段

| 阶段 | 值 |
|------|-----|
| `CompilerPhase.Preprocess` | `"preprocess"` |
| `CompilerPhase.SyntaxOnly` | `"syntax-only"` |
| `CompilerPhase.Compile` | `"compile"` |
| `CompilerPhase.Link` | `"link"` |
| `CompilerPhase.Archive` | `"archive"` |
| `CompilerPhase.RelocatableLink` | `"relocatable-link"` |
| `CompilerPhase.DeviceLink` | `"device-link"` |

### 支持的产物类型

| 产物 | 值 |
|------|-----|
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

分析归档命令（`ar`、`llvm-ar`、`gcc-ar`）。

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

### 实例属性

| 属性 | 类型 | 描述 |
|------|------|------|
| `operation` | `ArchiverOperation` | 归档操作（`"r"`、`"x"`、`"t"` 等） |
| `modifiers` | `string[]` | 修饰符字母（如 `["c", "s"]`） |
| `thin` | `boolean` | 是否请求了 thin 归档模式 |
| `archive` | `string \| undefined` | 归档文件路径 |
| `members` | `string[]` | 成员文件路径 |

### 支持的操作

分析器目前支持以下操作：

| 操作 | 值 | 描述 |
|------|-----|------|
| `ArchiverOperation.Print` | `"p"` | 打印成员 |
| `ArchiverOperation.QuickAppend` | `"q"` | 快速追加 |
| `ArchiverOperation.ReplaceOrInsert` | `"r"` | 替换或插入成员 |
| `ArchiverOperation.Table` | `"t"` | 列出内容 |

其他 `ar` 操作（`d`、`m`、`s`、`x`）在枚举中已定义，但 `ArchiverAnalysis.analyze()` 对它们会返回 `undefined`。

## Registry

分析器被组织在 `Registry` 中。默认注册表包含 `CompilerAnalysis` 和 `ArchiverAnalysis`。

### 使用默认注册表

```js
import { cmd } from "catter";

// 使用所有内置分析器进行分析
const analysis = cmd.analyze(["clang", "-c", "main.c"]);

// 检查是否有分析器能处理该命令
const canHandle = cmd.canHandle(["clang", "-c", "main.c"]);
```

### 自定义注册表

```js
import { cmd } from "catter";

const registry = new cmd.Registry();
registry.register(cmd.CompilerAnalysis);
registry.register(cmd.ArchiverAnalysis);

const analysis = registry.analyze(["gcc", "-c", "main.c"]);
```

### 添加自定义分析器

实现 `Analyzer` 接口，提供 `key`、`supports()` 和 `analyze()`：

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

// 注册到全局注册表
cmd.register(MyToolAnalysis);

// 或使用独立的注册表
const registry = new cmd.Registry();
registry.register(MyToolAnalysis);
```

### 收窄分析结果

从注册表获取通用 `Analysis` 后，使用静态 `from()` 方法进行类型收窄：

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
