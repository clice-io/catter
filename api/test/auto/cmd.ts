import {
  Analysis,
  AnalysisError,
  AnalyzedData,
  Analyzer,
  ArchiverAnalysis,
  ArchiverAnalyzer,
  ArchiverOperation,
  ArchiverUnsupportedError,
  CompilerAnalysis,
  CompilerAnalyzer,
  CompilerArtifact,
  CompilerPhase,
  Edge,
  Registry,
  type CommandAnalysis,
  type CommandAnalyzerError,
} from "catter/cmd";
import { assertThrow } from "catter/debug";
import { err, ok, type Result } from "catter/neverthrow";

function expectEq<T>(actual: T, expected: T, label: string): void {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function expectArrayEq(
  actual: readonly string[],
  expected: readonly string[],
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

function expectEdgeEq(actual: Edge, expected: Edge, label: string): void {
  expectEq(actual.output, expected.output, `${label} output`);
  expectArrayEq(actual.inputs, expected.inputs, `${label} inputs`);
}

function invocation(argv: string[], exe = argv[0]!): AnalyzedData {
  return {
    exe,
    argv,
  };
}

function expectOk<T, E>(result: Result<T, E>, label: string): T {
  assertThrow(result.isOk());
  if (result.isErr()) {
    throw new Error(`${label}: expected ok, got ${result.error}`);
  }
  return result.value;
}

function expectErr<T, E>(result: Result<T, E>, label: string): E {
  assertThrow(result.isErr());
  if (result.isOk()) {
    throw new Error(`${label}: expected error`);
  }
  return result.error;
}

function expectCompilerAnalysis<T extends Analysis, E>(
  result: Result<T, E>,
  label: string,
): CompilerAnalysis {
  const analysis = expectOk(result, label);
  if (!isCompilerAnalysis(analysis)) {
    throw new Error(`${label}: expected compiler analysis`);
  }
  return analysis;
}

function expectArchiverAnalysis<T extends Analysis, E>(
  result: Result<T, E>,
  label: string,
): ArchiverAnalysis {
  const analysis = expectOk(result, label);
  if (!isArchiverAnalysis(analysis)) {
    throw new Error(`${label}: expected archiver analysis`);
  }
  return analysis;
}

function isCompilerAnalysis(analysis: Analysis): analysis is CompilerAnalysis {
  return analysis.kind === "compiler";
}

function isArchiverAnalysis(analysis: Analysis): analysis is ArchiverAnalysis {
  return analysis.kind === "archiver";
}

const compilerAnalyzer = new CompilerAnalyzer();
const archiverAnalyzer = new ArchiverAnalyzer();
const builtinRegistry = new Registry<CommandAnalysis, CommandAnalyzerError>()
  .register("compiler", compilerAnalyzer)
  .register("archiver", archiverAnalyzer);

const compileCommand = ["clang", "-c", "src/a.c", "src/b.c"];
const compileAnalysis = expectCompilerAnalysis(
  builtinRegistry.analyze(invocation(compileCommand)),
  "compile analysis",
);

expectEq(compileAnalysis.kind, "compiler", "compile kind");
expectEq(compileAnalysis.exe, "clang", "compile exe");
expectArrayEq(compileAnalysis.argv, compileCommand, "compile argv");
expectEq(
  compileAnalysis.compilerMode.phase,
  CompilerPhase.Compile,
  "compile phase",
);
expectEq(
  compileAnalysis.compilerMode.artifact,
  CompilerArtifact.Object,
  "compile artifact",
);
expectArrayEq(compileAnalysis.reads, ["src/a.c", "src/b.c"], "compile reads");
expectArrayEq(compileAnalysis.writes, ["a.o", "b.o"], "compile writes");
expectArrayEq(
  compileAnalysis.sourceFiles,
  ["src/a.c", "src/b.c"],
  "compile sources",
);

const genericCompileAnalysis = builtinRegistry.analyze(
  invocation(compileCommand),
);
if (
  genericCompileAnalysis.isErr() ||
  genericCompileAnalysis.value.kind !== "compiler"
) {
  throw new Error("expected compiler command analysis variant");
}
expectArrayEq(
  genericCompileAnalysis.value.sourceFiles,
  ["src/a.c", "src/b.c"],
  "generic compile sources",
);

const compileEdges = compileAnalysis.edges;
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

const preprocessAnalysis = expectCompilerAnalysis(
  builtinRegistry.analyze(invocation(["gcc", "-E", "src/a.c", "-o", "a.i"])),
  "preprocess analysis",
);
expectEq(
  preprocessAnalysis.compilerMode.phase,
  CompilerPhase.Preprocess,
  "preprocess phase",
);
expectEq(
  preprocessAnalysis.compilerMode.artifact,
  CompilerArtifact.PreprocessedSource,
  "preprocess artifact",
);
expectArrayEq(preprocessAnalysis.reads, ["src/a.c"], "preprocess reads");
expectArrayEq(preprocessAnalysis.writes, ["a.i"], "preprocess writes");
expectEq(preprocessAnalysis.edges.length, 1, "preprocess edge count");

const archiverAnalysis = expectArchiverAnalysis(
  builtinRegistry.analyze(
    invocation(["llvm-ar", "--thin", "rcs", "libfoo.a", "a.o", "b.o"]),
  ),
  "archiver analysis",
);

expectEq(archiverAnalysis.kind, "archiver", "archiver kind");
expectEq(archiverAnalysis.exe, "llvm-ar", "archiver exe");
expectArrayEq(
  archiverAnalysis.argv,
  ["llvm-ar", "--thin", "rcs", "libfoo.a", "a.o", "b.o"],
  "archiver argv",
);
expectEq(
  archiverAnalysis.operation,
  ArchiverOperation.ReplaceOrInsert,
  "archiver operation",
);
assertThrow(archiverAnalysis.thin);
expectArrayEq(archiverAnalysis.reads, ["a.o", "b.o"], "archiver reads");
expectArrayEq(archiverAnalysis.writes, ["libfoo.a"], "archiver writes");

const archiveEdges = archiverAnalysis.edges;
expectEq(archiveEdges.length, 1, "archiver edge count");
expectEdgeEq(
  archiveEdges[0],
  {
    output: "libfoo.a",
    inputs: ["a.o", "b.o"],
  },
  "archiver edge",
);

const gnuArchiverAnalysis = expectArchiverAnalysis(
  builtinRegistry.analyze(
    invocation(["ar", "-cr", "libcommon.a", "a.o", "b.o"]),
  ),
  "gnu archiver analysis",
);
expectEq(
  gnuArchiverAnalysis.operation,
  ArchiverOperation.ReplaceOrInsert,
  "gnu archiver operation",
);
expectArrayEq(gnuArchiverAnalysis.modifiers, ["c"], "gnu archiver modifiers");
expectArrayEq(
  gnuArchiverAnalysis.writes,
  ["libcommon.a"],
  "gnu archiver writes",
);
expectArrayEq(gnuArchiverAnalysis.reads, ["a.o", "b.o"], "gnu archiver reads");

const tableArchiverAnalysis = expectArchiverAnalysis(
  builtinRegistry.analyze(invocation(["ar", "t", "libcommon.a"])),
  "table archiver analysis",
);
expectEq(
  tableArchiverAnalysis.operation,
  ArchiverOperation.Table,
  "table archiver operation",
);
expectArrayEq(
  tableArchiverAnalysis.reads,
  ["libcommon.a"],
  "table archiver reads",
);
expectArrayEq(tableArchiverAnalysis.writes, [], "table archiver writes");

assertThrow(
  expectErr(
    archiverAnalyzer.analyze(invocation(["ar", "--version"])),
    "archiver version",
  ) instanceof ArchiverUnsupportedError,
);
assertThrow(
  expectErr(
    archiverAnalyzer.analyze(invocation(["ar", "x", "libcommon.a"])),
    "archiver extract",
  ) instanceof ArchiverUnsupportedError,
);

class ToyAnalysis extends Analysis {
  readonly stage = "bundle";
  readonly kind = "toy" as const;

  constructor(command: AnalyzedData, input: string, output: string) {
    super({
      exe: command.exe,
      argv: command.argv,
      reads: [input],
      writes: [output],
      edges: [
        {
          output,
          inputs: [input],
        },
      ],
    });
  }
}

class ToyAnalysisError extends AnalysisError {
  readonly kind = "toy" as const;
}

class ToyAnalyzer extends Analyzer {
  readonly kind = "toy" as const;

  analyze(command: AnalyzedData): Result<ToyAnalysis, AnalysisError> {
    const argv = command.argv;
    if (
      command.exe !== "toy-bundle" ||
      argv[1] === undefined ||
      argv[2] === undefined
    ) {
      return err(new ToyAnalysisError("not a toy command"));
    }
    return ok(new ToyAnalysis(command, argv[1], argv[2]));
  }
}

const toyAnalyzer = new ToyAnalyzer();
const localRegistry = new Registry<ToyAnalysis, AnalysisError>().register(
  "toy",
  toyAnalyzer,
);
const localResult = expectOk(
  localRegistry.analyze(invocation(["toy-bundle", "input.dat", "output.pkg"])),
  "local registry",
);
assertThrow(localResult.kind === "toy");
expectEq(localResult.stage, "bundle", "local stage");
expectEq(localResult.exe, "toy-bundle", "local exe");
expectArrayEq(
  localResult.argv,
  ["toy-bundle", "input.dat", "output.pkg"],
  "local argv",
);
expectArrayEq(localResult.reads, ["input.dat"], "local reads");
expectArrayEq(localResult.writes, ["output.pkg"], "local writes");
expectEq(localResult.edges.length, 1, "local edge count");
expectEq(localResult.edges[0].output, "output.pkg", "local edge output");
localRegistry.unregister("toy");
assertThrow(
  localRegistry
    .analyze(invocation(["toy-bundle", "input.dat", "output.pkg"]))
    .isErr(),
);

const sample = expectCompilerAnalysis(
  compilerAnalyzer.analyze(invocation(["gcc", "-c", "sample.c"])),
  "sample compiler analysis",
);
expectArrayEq(sample.writes, ["sample.o"], "sample writes");

type LocalAnalysis = CompilerAnalysis | ToyAnalysis;
const mixedRegistry = new Registry<LocalAnalysis, AnalysisError>().register(
  "compiler",
  compilerAnalyzer,
);
const mixedResult = expectOk(
  mixedRegistry.analyze(invocation(["gcc", "-c", "mixed.c"])),
  "mixed registry",
);
if (mixedResult.kind === "compiler") {
  expectArrayEq(mixedResult.writes, ["mixed.o"], "mixed compiler writes");
} else {
  expectEq(mixedResult.stage, "bundle", "mixed toy stage");
}

assertThrow(
  mixedRegistry
    .analyze(invocation(["toy-bundle", "input.dat", "output.pkg"]))
    .isErr(),
);
