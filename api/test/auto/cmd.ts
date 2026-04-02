import { cmd, debug } from "catter";

function expectEq<T>(actual: T, expected: T, label: string): void {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function expectArrayEq(
  actual: string[],
  expected: string[],
  label: string,
): void {
  if (actual.length !== expected.length) {
    throw new Error(
      `${label}: expected [${expected.join(", ")}], got [${actual.join(", ")}]`,
    );
  }

  for (let index = 0; index < actual.length; ++index) {
    if (actual[index] !== expected[index]) {
      throw new Error(
        `${label}: expected [${expected.join(", ")}], got [${actual.join(", ")}]`,
      );
    }
  }
}

function expectEdgeEq(
  actual: cmd.Edge,
  expected: cmd.Edge,
  label: string,
): void {
  expectEq(actual.output, expected.output, `${label} output`);
  expectArrayEq(actual.inputs, expected.inputs, `${label} inputs`);
}

const compileCommand = ["clang", "-c", "src/a.c", "src/b.c"];
const compileAnalysis = cmd.CompilerAnalysis.from(cmd.analyze(compileCommand));
debug.assertThrow(compileAnalysis !== undefined);
if (compileAnalysis === undefined) {
  throw new Error("expected compiler analysis");
}

expectEq(compileAnalysis.exe, "clang", "compile exe");
expectEq(compileAnalysis.compiler, "clang", "compile compiler");
expectEq(compileAnalysis.phase, cmd.CompilerPhase.Compile, "compile phase");
expectEq(
  compileAnalysis.artifact,
  cmd.CompilerArtifact.Object,
  "compile artifact",
);
expectArrayEq(
  compileAnalysis.consume,
  ["src/a.c", "src/b.c"],
  "compile consume",
);
expectArrayEq(compileAnalysis.produce, ["a.o", "b.o"], "compile produce");
expectArrayEq(
  compileAnalysis.sourceInputs(),
  ["src/a.c", "src/b.c"],
  "compile sources",
);

const compileEdges = compileAnalysis.edges();
expectEq(compileEdges.length, 2, "compile edge count");
expectEdgeEq(
  compileEdges[0],
  {
    output: "a.o",
    inputs: ["src/a.c"],
  },
  "compile first edge",
);
expectEdgeEq(
  compileEdges[1],
  {
    output: "b.o",
    inputs: ["src/b.c"],
  },
  "compile second edge",
);

const cdbItems = cmd.cdbItemsOf(
  {
    cwd: "/tmp/build",
    argv: [...compileCommand],
  },
  [
    {
      file: "src/a.c",
      output: "a.o",
    },
    {
      file: "src/b.c",
      output: "b.o",
    },
  ],
);
expectEq(cdbItems.length, 2, "cdb item count");
expectEq(cdbItems[0].directory, "/tmp/build", "cdb directory");
expectEq(cdbItems[0].file, "src/a.c", "cdb first file");
expectEq(cdbItems[0].output, "a.o", "cdb first output");

const preprocessAnalysis = cmd.CompilerAnalysis.from(
  cmd.analyze(["gcc", "-E", "src/a.c", "-o", "a.i"]),
);
debug.assertThrow(preprocessAnalysis !== undefined);
if (preprocessAnalysis === undefined) {
  throw new Error("expected preprocess compiler analysis");
}
expectEq(
  preprocessAnalysis.phase,
  cmd.CompilerPhase.Preprocess,
  "preprocess phase",
);
expectEq(
  preprocessAnalysis.artifact,
  cmd.CompilerArtifact.Stdout,
  "preprocess artifact",
);
expectArrayEq(preprocessAnalysis.consume, ["src/a.c"], "preprocess consume");
expectArrayEq(preprocessAnalysis.produce, ["a.i"], "preprocess produce");
expectEq(preprocessAnalysis.edges().length, 1, "preprocess edge count");

const archiverAnalysis = cmd.ArchiverAnalysis.from(
  cmd.analyze(["llvm-ar", "--thin", "rcs", "libfoo.a", "a.o", "b.o"]),
);
debug.assertThrow(archiverAnalysis !== undefined);
if (archiverAnalysis === undefined) {
  throw new Error("expected archiver analysis");
}

expectEq(archiverAnalysis.exe, "llvm-ar", "archiver exe");
expectEq(
  archiverAnalysis.operation,
  cmd.ArchiverOperation.ReplaceOrInsert,
  "archiver operation",
);
debug.assertThrow(archiverAnalysis.thin);
expectArrayEq(archiverAnalysis.consume, ["a.o", "b.o"], "archiver consume");
expectArrayEq(archiverAnalysis.produce, ["libfoo.a"], "archiver produce");

const archiveEdges = archiverAnalysis.edges();
expectEq(archiveEdges.length, 1, "archiver edge count");
expectEdgeEq(
  archiveEdges[0],
  {
    output: "libfoo.a",
    inputs: ["a.o", "b.o"],
  },
  "archiver edge",
);

const gnuArchiverAnalysis = cmd.ArchiverAnalysis.from(
  cmd.analyze(["ar", "-cr", "libcommon.a", "a.o", "b.o"]),
);
debug.assertThrow(gnuArchiverAnalysis !== undefined);
if (gnuArchiverAnalysis === undefined) {
  throw new Error("expected gnu archiver analysis");
}
expectEq(gnuArchiverAnalysis.exe, "ar", "gnu archiver exe");
expectEq(
  gnuArchiverAnalysis.operation,
  cmd.ArchiverOperation.ReplaceOrInsert,
  "gnu archiver operation",
);
expectArrayEq(gnuArchiverAnalysis.modifiers, ["c"], "gnu archiver modifiers");
expectArrayEq(
  gnuArchiverAnalysis.produce,
  ["libcommon.a"],
  "gnu archiver produce",
);

class ToyAnalysis extends cmd.Analysis<"toy-bundle"> {
  static readonly key = "toy-bundle";

  static supports(argv: readonly string[]): boolean {
    return argv[0] === "toy-bundle";
  }

  static analyze(argv: readonly string[]): ToyAnalysis | undefined {
    if (
      !ToyAnalysis.supports(argv) ||
      argv[1] === undefined ||
      argv[2] === undefined
    ) {
      return undefined;
    }
    return new ToyAnalysis(argv[1], argv[2]);
  }

  static from(analysis: cmd.Analysis | undefined): ToyAnalysis | undefined {
    return analysis instanceof ToyAnalysis ? analysis : undefined;
  }

  readonly stage = "bundle";

  constructor(input: string, output: string) {
    super("toy-bundle", [input], [output]);
  }
}

const localRegistry = new cmd.Registry().register(ToyAnalysis);
const localResult = ToyAnalysis.from(
  localRegistry.analyze(["toy-bundle", "input.dat", "output.pkg"]),
);
debug.assertThrow(localResult !== undefined);
if (localResult === undefined) {
  throw new Error("expected local analysis");
}
expectEq(localResult.stage, "bundle", "local stage");
expectArrayEq(localResult.consume, ["input.dat"], "local consume");
expectArrayEq(localResult.produce, ["output.pkg"], "local produce");
expectEq(localResult.edges().length, 1, "local edge count");
expectEq(localResult.edges()[0].output, "output.pkg", "local edge output");

const compat = new cmd.CompilerAnalysis(["gcc", "-c", "sample.c"]);
expectEq(compat.compiler, "gcc", "compat compiler");
expectArrayEq(compat.outputs(), ["sample.o"], "compat outputs");
