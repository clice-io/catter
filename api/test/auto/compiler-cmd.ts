import * as cmd from "catter/cmd";
import * as debug from "catter/debug";
import * as fs from "catter/fs";
import * as neverthrow from "catter/neverthrow";

type ExpectedAnalysis = {
  label: string;
  cmd: string[];
  compilerMode: cmd.CompilerMode;
  inputs: string[];
  outputs: string[];
  target?: {
    artifactModel: cmd.CompilerArtifactModel;
    source: cmd.CompilerTargetSource["kind"];
    triple?: string;
  };
};

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function expectArrayEq(
  actual: readonly string[],
  expected: readonly string[],
  label: string,
) {
  if (actual.length !== expected.length) {
    throw new Error(
      `${label}: expected [${expected.join(", ")}], got [${actual.join(", ")}]`,
    );
  }

  for (let idx = 0; idx < actual.length; ++idx) {
    if (actual[idx] !== expected[idx]) {
      throw new Error(
        `${label}: expected [${expected.join(", ")}], got [${actual.join(", ")}]`,
      );
    }
  }
}

function normalizedJoin(...parts: string[]) {
  return fs.path.lexicalNormal(fs.path.joinAll(...parts));
}

function invocation(argv: string[], exe = argv[0]!): cmd.AnalyzedData {
  return {
    exe,
    argv,
  };
}

function expectCompilerAnalysis(
  result: ReturnType<cmd.CompilerAnalyzer["analyze"]>,
  label: string,
): cmd.CompilerAnalysis {
  debug.assertThrow(result.isOk());
  if (result.isErr()) {
    throw new Error(
      `${label}: expected compiler analysis, got ${result.error}`,
    );
  }
  const analysis = result.value;
  if (!isCompilerAnalysis(analysis)) {
    throw new Error(`${label}: expected compiler analysis`);
  }
  return analysis;
}

function isCompilerAnalysis(
  analysis: cmd.Analysis,
): analysis is cmd.CompilerAnalysis {
  return analysis.kind === "compiler";
}

function expectAnalysisError(
  result: ReturnType<cmd.CompilerAnalyzer["analyze"]>,
  label: string,
): cmd.AnalysisError {
  debug.assertThrow(result.isErr());
  if (result.isOk()) {
    throw new Error(`${label}: expected analysis error`);
  }
  debug.assertThrow(result.error instanceof Error);
  debug.assertThrow(result.error instanceof cmd.AnalysisError);
  return result.error;
}

const compilerIdentifier = new cmd.CompilerIdentifier();
const compilerAnalyzer = new cmd.CompilerAnalyzer({
  identifier: compilerIdentifier,
});

function parseCompilerCommand(argv: string[]): cmd.CompilerParseResult {
  return cmd.parseCompilerCommand(
    argv,
    compilerIdentifier.identifyCompilerCommand(invocation(argv)),
  );
}

function resolveCompilerCommand(
  argv: string[],
  resolver: cmd.CompilerResolver,
): cmd.CompilerResolveResult {
  const identity = compilerIdentifier.identifyCompilerCommand(invocation(argv));
  return resolver.resolve(cmd.parseCompilerCommand(argv, identity), identity);
}

function expectAnalysis(expected: ExpectedAnalysis) {
  const analysis = expectCompilerAnalysis(
    compilerAnalyzer.analyze(invocation(expected.cmd)),
    expected.label,
  );

  expectEq(analysis.kind, "compiler", `${expected.label} kind`);
  expectEq(analysis.exe, expected.cmd[0], `${expected.label} exe`);
  expectArrayEq(analysis.argv, expected.cmd, `${expected.label} argv`);
  expectEq(
    analysis.unwrappedExe,
    expected.cmd[0],
    `${expected.label} unwrapped exe`,
  );
  expectArrayEq(
    analysis.unwrappedArgv,
    expected.cmd,
    `${expected.label} unwrapped argv`,
  );
  expectEq(
    analysis.compilerMode.phase,
    expected.compilerMode.phase,
    `${expected.label} phase`,
  );
  expectEq(
    analysis.compilerMode.artifact,
    expected.compilerMode.artifact,
    `${expected.label} artifact`,
  );
  expectArrayEq(analysis.reads, expected.inputs, `${expected.label} reads`);
  expectArrayEq(analysis.writes, expected.outputs, `${expected.label} writes`);
  if (expected.target !== undefined) {
    expectEq(
      analysis.target.artifactModel,
      expected.target.artifactModel,
      `${expected.label} target artifact model`,
    );
    expectEq(
      analysis.target.source.kind,
      expected.target.source,
      `${expected.label} target source`,
    );
    expectEq(
      analysis.target.descriptor.triple,
      expected.target.triple,
      `${expected.label} target triple`,
    );
  }
}

debug.assertThrow(
  compilerAnalyzer.analyze(invocation(["clang", "-c", "main.cc"])).isOk(),
);
debug.assertThrow(
  compilerAnalyzer.analyze(invocation(["gcc", "-c", "main.cc"])).isOk(),
);
debug.assertThrow(
  compilerAnalyzer.analyze(invocation(["clang-cl", "/c", "main.cc"])).isOk(),
);
debug.assertThrow(
  compilerAnalyzer.analyze(invocation(["cl.exe", "/c", "main.cc"])).isOk(),
);
debug.assertThrow(
  expectAnalysisError(
    compilerAnalyzer.analyze(invocation(["nvcc", "-c", "kernel.cu"])),
    "nvcc",
  ) instanceof cmd.CompilerUnsupportedError,
);
const nvccIdentity = compilerIdentifier.identifyCompilerCommand(
  invocation(["nvcc", "-c", "kernel.cu"]),
);
debug.assertThrow(nvccIdentity?.dialect === cmd.CompilerDialect.Nvcc);

const unprefixedVersionedClangIdentity =
  compilerIdentifier.identifyCompilerCommand(
    invocation(["clang-20", "-c", "main.c"]),
  );
expectEq(
  unprefixedVersionedClangIdentity.target,
  undefined,
  "versioned clang has no executable target",
);

const prefixedVersionedClangIdentity =
  compilerIdentifier.identifyCompilerCommand(
    invocation(["aarch64-linux-gnu-clang-20", "-c", "main.c"]),
  );
expectEq(
  prefixedVersionedClangIdentity.target?.target.triple,
  "aarch64-linux-gnu",
  "versioned cross clang executable target",
);
expectEq(
  prefixedVersionedClangIdentity.target?.source.kind,
  "executable-prefix",
  "versioned cross clang target source",
);

const absoluteCrossClangIdentity = compilerIdentifier.identifyCompilerCommand(
  invocation(
    ["x86_64-w64-mingw32-clang++.exe", "-c", "main.cpp"],
    "C:/toolchains/bin/x86_64-w64-mingw32-clang++.exe",
  ),
);
expectEq(
  absoluteCrossClangIdentity.target?.target.triple,
  "x86_64-w64-mingw32",
  "absolute cross clang executable target",
);

const absoluteExeAnalysis = expectCompilerAnalysis(
  compilerAnalyzer.analyze(
    invocation(["gcc", "-c", "absolute.c"], "/opt/toolchains/bin/gcc"),
  ),
  "absolute executable compiler analysis",
);
expectEq(
  absoluteExeAnalysis.exe,
  "/opt/toolchains/bin/gcc",
  "absolute executable path",
);
expectArrayEq(
  absoluteExeAnalysis.writes,
  ["absolute.o"],
  "absolute executable writes",
);

const parserIr = parseCompilerCommand([
  "clang",
  "-S",
  "-emit-llvm",
  "src/t.c",
  "-o",
  "-",
]);
expectEq(
  parserIr.compilerMode.artifact,
  cmd.CompilerArtifact.LlvmIR,
  "parser ir artifact",
);
expectEq(parserIr.compilerActions.length, 2, "parser ir action count");
expectEq(
  parserIr.compilerActions[0]!.kind,
  "compile-assembly-like",
  "parser ir first action",
);
expectEq(
  parserIr.compilerActions[1]!.kind,
  "compile-llvm-like",
  "parser ir second action",
);
expectEq(parserIr.inputs.length, 0, "parser ir definite input count");
expectEq(parserIr.inputCandidates.length, 1, "parser ir input candidate count");
expectEq(
  parserIr.inputCandidates[0]!.path,
  "src/t.c",
  "parser ir input candidate path",
);
expectEq(parserIr.outputs.length, 1, "parser ir output fact count");
expectEq(
  parserIr.outputCandidates.length,
  0,
  "parser ir output candidate count",
);
expectEq(parserIr.outputs[0]!.path, "-", "parser ir output fact path");

const parserTarget = parseCompilerCommand([
  "clang",
  "--target=aarch64-linux-gnu",
  "--target=x86_64-pc-windows-msvc",
  "-c",
  "main.c",
]);
expectEq(
  parserTarget.target?.target.triple,
  "x86_64-pc-windows-msvc",
  "parser final explicit target",
);
expectEq(
  parserTarget.target?.source.kind,
  "argument",
  "parser explicit target source",
);
if (parserTarget.target?.source.kind === "argument") {
  expectEq(
    parserTarget.target.source.index,
    1,
    "parser final explicit target index",
  );
}

const remainderIr = parseCompilerCommand([
  "clang",
  "--driver-mode=cl",
  "foo.obj",
  "/link",
  "/dll",
  "/out:bin/tool.dll",
  "bar.res",
]);
expectEq(
  remainderIr.compilerActions[0]!.index,
  3,
  "parser ir linker remainder action index",
);
expectEq(
  remainderIr.outputs[0]!.index,
  4,
  "parser ir linker remainder output index",
);
expectEq(
  remainderIr.inputs[0]!.index,
  5,
  "parser ir linker remainder input index",
);
expectEq(
  remainderIr.outputs[0]!.kind,
  "linked-artifact",
  "parser ir linker remainder output kind",
);
expectEq(
  remainderIr.inputs[0]!.source.kind,
  "remainder-argument",
  "parser ir linker remainder input source",
);

const assemblyListingIr = parseCompilerCommand([
  "clang",
  "--driver-mode=cl",
  "/FA",
  "/Faasm/",
  "src/main.cpp",
]);
expectEq(
  assemblyListingIr.compilerActions.length,
  2,
  "parser ir assembly listing action count",
);
expectEq(
  assemblyListingIr.compilerActions[0]!.kind,
  "emit-assembly-listing",
  "parser ir first assembly listing action",
);
expectEq(
  assemblyListingIr.compilerActions[1]!.kind,
  "emit-assembly-listing",
  "parser ir second assembly listing action",
);
const assemblyListingPathAction = assemblyListingIr.compilerActions[1]!;
if (assemblyListingPathAction.kind !== "emit-assembly-listing") {
  throw new Error("parser ir expected assembly listing path action");
}
expectEq(
  assemblyListingPathAction.path,
  "asm/",
  "parser ir assembly listing path",
);
expectEq(
  assemblyListingIr.outputs.length,
  0,
  "parser ir assembly listing output fact count",
);

const unknownOptionIr = parseCompilerCommand([
  "clang",
  "-c",
  "--definitely-not-a-real-clang-flag",
  "not-a-source",
  "src/main.c",
]);
expectEq(
  unknownOptionIr.inputs.length,
  0,
  "parser ir unknown option definite input count",
);
expectEq(
  unknownOptionIr.inputCandidates.length,
  2,
  "parser ir unknown option input candidate count",
);
expectEq(
  unknownOptionIr.inputCandidates[0]!.path,
  "not-a-source",
  "parser ir unknown option value candidate path",
);
expectEq(
  unknownOptionIr.inputCandidates[1]!.path,
  "src/main.c",
  "parser ir unknown option source candidate path",
);

const debugResolverCmd = ["clang", "-c", "not-a-source"];

const debugResolverParsed = parseCompilerCommand(debugResolverCmd);

const debugResolverresolved = new cmd.CompilerResolver({
  debug: true,
}).resolve(
  debugResolverParsed,
  compilerIdentifier.identifyCompilerCommand(invocation(debugResolverCmd)),
);

expectArrayEq(
  debugResolverresolved.reads,
  [],
  "resolver debug rejected unknown candidate reads",
);

if (debugResolverresolved.debug === undefined) {
  throw new Error("resolver debug information missing");
}
expectEq(
  debugResolverresolved.debug.inputCandidates.length,
  1,
  "resolver debug rejected candidate count",
);

expectEq(
  debugResolverresolved.debug.inputCandidates[0]!.decision,
  "rejected",
  "resolver debug candidate decision",
);

const allCandidateAnalyzer = new cmd.CompilerAnalyzer({
  resolver: new cmd.CompilerResolver({
    inputCandidates: {
      withoutLanguage: {
        unknownSuffix: "source",
      },
    },
  }),
});
const allCandidateAnalysis = expectCompilerAnalysis(
  allCandidateAnalyzer.analyze(invocation(["clang", "-c", "not-a-source"])),
  "resolver accepts unknown candidates as source",
);
expectArrayEq(
  allCandidateAnalysis.reads,
  ["not-a-source"],
  "resolver accepts unknown candidates reads",
);
expectArrayEq(
  allCandidateAnalysis.writes,
  ["not-a-source.o"],
  "resolver accepts unknown candidates writes",
);

const noCandidateAnalyzer = new cmd.CompilerAnalyzer({
  resolver: new cmd.CompilerResolver({
    inputCandidates: {
      byLanguage: {
        c: {
          suffixRules: [],
          unknownSuffix: "reject",
        },
        "c++": {
          suffixRules: [],
          unknownSuffix: "reject",
        },
      },
      withoutLanguage: {
        suffixRules: [],
        unknownSuffix: "reject",
      },
    },
  }),
});
const noCandidateAnalysis = expectCompilerAnalysis(
  noCandidateAnalyzer.analyze(invocation(["clang", "-c", "main.c"])),
  "resolver ignores candidates",
);
expectArrayEq(
  noCandidateAnalysis.reads,
  [],
  "resolver ignores candidates reads",
);
expectArrayEq(
  noCandidateAnalysis.writes,
  [],
  "resolver ignores candidates writes",
);

const noDefaultOutputAnalyzer = new cmd.CompilerAnalyzer({
  resolver: new cmd.CompilerResolver({
    writes: {
      inferDefaultOutputs: false,
    },
  }),
});
const noDefaultOutputAnalysis = expectCompilerAnalysis(
  noDefaultOutputAnalyzer.analyze(invocation(["clang", "-c", "main.c"])),
  "resolver disables default outputs",
);
expectArrayEq(
  noDefaultOutputAnalysis.reads,
  ["main.c"],
  "resolver disables default outputs reads",
);
expectArrayEq(
  noDefaultOutputAnalysis.writes,
  [],
  "resolver disables default outputs writes",
);

const unresolvedImplicitOutput = resolveCompilerCommand(
  ["clang", "main.c"],
  new cmd.CompilerResolver({ debug: true }),
);
expectArrayEq(
  unresolvedImplicitOutput.writes,
  [],
  "resolver unresolved implicit output writes",
);
if (unresolvedImplicitOutput.debug === undefined) {
  throw new Error(
    "resolver unresolved implicit output debug information missing",
  );
}
expectEq(
  unresolvedImplicitOutput.debug.diagnostics.length,
  1,
  "resolver unresolved implicit output diagnostic count",
);
expectEq(
  unresolvedImplicitOutput.debug.diagnostics[0]!.code,
  "implicit-output-unresolved",
  "resolver unresolved implicit output diagnostic code",
);
const unresolvedImplicitOutputWithoutDebug = resolveCompilerCommand(
  ["clang", "main.c"],
  new cmd.CompilerResolver(),
);
expectEq(
  unresolvedImplicitOutputWithoutDebug.debug,
  undefined,
  "resolver unresolved implicit output hides diagnostics without debug",
);

const noDirectoryExpansionAnalyzer = new cmd.CompilerAnalyzer({
  resolver: new cmd.CompilerResolver({
    writes: {
      expandDirectoryOutputs: false,
    },
  }),
});
const noDirectoryExpansionAnalysis = expectCompilerAnalysis(
  noDirectoryExpansionAnalyzer.analyze(
    invocation(["clang-cl", "/c", "/Tp", "src/noext", "/Fo:build/"]),
  ),
  "resolver disables directory output expansion",
);
expectArrayEq(
  noDirectoryExpansionAnalysis.writes,
  ["build/"],
  "resolver disables directory output expansion writes",
);

const noAssemblyListingAnalyzer = new cmd.CompilerAnalyzer({
  resolver: new cmd.CompilerResolver({
    writes: {
      inferAssemblyListings: false,
    },
  }),
});
const noAssemblyListingAnalysis = expectCompilerAnalysis(
  noAssemblyListingAnalyzer.analyze(
    invocation(["clang-cl", "/c", "/FA", "src/main.cpp"]),
  ),
  "resolver disables assembly listings",
);
expectArrayEq(
  noAssemblyListingAnalysis.writes,
  ["main.obj"],
  "resolver disables assembly listings writes",
);

let incompleteConventionRejected = false;
try {
  resolveCompilerCommand(
    ["clang", "-c", "main.c"],
    new cmd.CompilerResolver({
      debug: true,
      targetOverride: {
        os: cmd.CompilerTargetOS.Unknown,
        env: cmd.CompilerTargetEnv.Unknown,
        objectFormat: cmd.CompilerObjectFormat.Unknown,
      },
    }),
  );
} catch (error) {
  incompleteConventionRejected =
    error instanceof cmd.CompilerTargetResolutionError;
}
debug.assertThrow(incompleteConventionRejected);

const unknownTargetWithConventionResolved = resolveCompilerCommand(
  ["clang", "-c", "main.c"],
  new cmd.CompilerResolver({
    debug: true,
    targetOverride: {
      os: cmd.CompilerTargetOS.Unknown,
      env: cmd.CompilerTargetEnv.Unknown,
      objectFormat: cmd.CompilerObjectFormat.Unknown,
      artifactModel: cmd.CompilerArtifactModel.Elf,
    },
    outputConvention: {
      object: ".objx",
      executable: ".binx",
      sharedLibrary: ".sox",
      staticLibrary: ".libx",
    },
  }),
);
expectArrayEq(
  unknownTargetWithConventionResolved.reads,
  ["main.c"],
  "resolver unknown target with convention reads",
);
expectArrayEq(
  unknownTargetWithConventionResolved.writes,
  ["main.objx"],
  "resolver unknown target with convention writes",
);

const customUnspecifiedSuffixAnalyzer = new cmd.CompilerAnalyzer({
  resolver: new cmd.CompilerResolver({
    inputCandidates: {
      withoutLanguage: {
        suffixRules: [{ suffix: ".foo", role: "source" }],
        unknownSuffix: "reject",
      },
    },
  }),
});
const customUnspecifiedSuffixAnalysis = expectCompilerAnalysis(
  customUnspecifiedSuffixAnalyzer.analyze(
    invocation(["clang", "-c", "main.foo"]),
  ),
  "resolver custom unspecified suffix source",
);
expectArrayEq(
  customUnspecifiedSuffixAnalysis.reads,
  ["main.foo"],
  "resolver custom unspecified suffix reads",
);
expectArrayEq(
  customUnspecifiedSuffixAnalysis.writes,
  ["main.o"],
  "resolver custom unspecified suffix writes",
);

const customExplicitSuffixAnalyzer = new cmd.CompilerAnalyzer({
  resolver: new cmd.CompilerResolver({
    inputCandidates: {
      byLanguage: {
        c: {
          suffixRules: [{ suffix: ".src", role: "source" }],
          unknownSuffix: "reject",
        },
      },
    },
  }),
});
const customExplicitSuffixAnalysis = expectCompilerAnalysis(
  customExplicitSuffixAnalyzer.analyze(
    invocation(["clang", "-x", "c", "-c", "main.src", "ignored-noext"]),
  ),
  "resolver custom explicit suffix source",
);
expectArrayEq(
  customExplicitSuffixAnalysis.reads,
  ["main.src"],
  "resolver custom explicit suffix reads",
);
expectArrayEq(
  customExplicitSuffixAnalysis.writes,
  ["main.o"],
  "resolver custom explicit suffix writes",
);

const cases: ExpectedAnalysis[] = [
  {
    label: "clang llvm ir explicit stdout output",
    cmd: ["clang", "src/t.c", "-S", "-emit-llvm", "-o", "-"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.LlvmIR,
    },
    inputs: ["src/t.c"],
    outputs: ["-"],
  },
  {
    label: "clang stdin input is not a filesystem read",
    cmd: ["clang", "-x", "c", "-c", "-", "-o", "stdin.o"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: [],
    outputs: ["stdin.o"],
  },
  {
    label: "clang stdin input has no default filesystem output",
    cmd: ["clang", "-x", "c", "-c", "-"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: [],
    outputs: [],
  },
  {
    label: "gcc preprocess explicit language rejects no-suffix candidate",
    cmd: ["gcc", "-x", "c", "not-a-source", "src/a.c", "-E", "-P"],
    compilerMode: {
      phase: cmd.CompilerPhase.Preprocess,
      artifact: cmd.CompilerArtifact.PreprocessedSource,
    },
    inputs: ["src/a.c"],
    outputs: [],
  },
  {
    label: "gcc preprocess to file",
    cmd: ["gcc", "-E", "src/a.c", "-o", "a.i"],
    compilerMode: {
      phase: cmd.CompilerPhase.Preprocess,
      artifact: cmd.CompilerArtifact.PreprocessedSource,
    },
    inputs: ["src/a.c"],
    outputs: ["a.i"],
  },
  {
    label: "gcc syntax-only explicit language",
    cmd: [
      "gcc",
      "-x",
      "c++",
      "not-a-source",
      "-fsyntax-only",
      "-fno-exceptions",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.SyntaxOnly,
      artifact: cmd.CompilerArtifact.None,
    },
    inputs: [],
    outputs: [],
  },
  {
    label: "gcc x-none resets classification for later object inputs",
    cmd: [
      "gcc",
      "-x",
      "c",
      "not-a-source",
      "-x",
      "none",
      "obj/plain.o",
      "-o",
      "bin/app",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["obj/plain.o"],
    outputs: ["bin/app"],
  },
  {
    label: "gcc relocatable link with extra linker flags",
    cmd: [
      "gcc",
      "-nostdlib",
      "-Wl,--build-id=sha1",
      "-r",
      "a.o",
      "b.o",
      "-o",
      "partial.o",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.RelocatableLink,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["a.o", "b.o"],
    outputs: ["partial.o"],
  },
  {
    label: "gcc compile link input has no default source output",
    cmd: ["gcc", "-x", "none", "obj/plain.o", "-c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["obj/plain.o"],
    outputs: [],
  },
  {
    label: "gcc joined output spelling",
    cmd: ["gcc", "-c", "src/joined.c", "-oobj/joined.o"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/joined.c"],
    outputs: ["obj/joined.o"],
  },
  {
    label: "clang llvm bitcode action survives object stop phase",
    cmd: ["clang", "-emit-llvm", "-c", "src/t.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.LlvmBitcode,
    },
    inputs: ["src/t.c"],
    outputs: ["t.bc"],
  },
  {
    label: "clang assembly action survives object stop phase",
    cmd: ["clang", "-S", "-c", "src/t.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Assembly,
    },
    inputs: ["src/t.c"],
    outputs: ["t.s"],
  },
  {
    label: "clang compile action survives shared link option",
    cmd: ["clang", "-c", "-shared", "src/t.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/t.c"],
    outputs: ["t.o"],
  },
  {
    label:
      "clang preprocess unknown option value without suffix is not an input",
    cmd: [
      "clang",
      "-E",
      "--definitely-not-a-real-clang-flag",
      "not-a-source",
      "src/main.c",
      "-o",
      "main.i",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Preprocess,
      artifact: cmd.CompilerArtifact.PreprocessedSource,
    },
    inputs: ["src/main.c"],
    outputs: ["main.i"],
  },
  {
    label: "clang unknown option value without suffix is not an input",
    cmd: [
      "clang",
      "-c",
      "--definitely-not-a-real-clang-flag",
      "not-a-source",
      "src/main.c",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/main.c"],
    outputs: ["main.o"],
  },
  {
    label: "clang link unknown option value without suffix is not an input",
    cmd: [
      "clang",
      "--definitely-not-a-real-clang-flag",
      "not-a-source",
      "obj/main.o",
      "-o",
      "bin/app",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["obj/main.o"],
    outputs: ["bin/app"],
  },
  {
    label: "gcc unknown option value without suffix is not an input",
    cmd: [
      "gcc",
      "--definitely-not-a-real-gcc-flag",
      "not-a-source",
      "src/main.c",
      "-o",
      "bin/app",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["src/main.c"],
    outputs: ["bin/app"],
  },
  {
    label: "gcc explicit language rejects unknown option value candidate",
    cmd: [
      "gcc",
      "-x",
      "c",
      "--definitely-not-a-real-gcc-flag",
      "not-a-source",
      "src/main.c",
      "-c",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/main.c"],
    outputs: ["main.o"],
  },
  {
    label: "clang unresolved default executable output",
    cmd: ["clang", "src/t.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["src/t.c"],
    outputs: [],
  },
  {
    label: "clang unresolved default executable output from object input",
    cmd: ["clang", "obj/t.o"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["obj/t.o"],
    outputs: [],
  },
  {
    label: "clang archive static lib from object inputs",
    cmd: ["clang", "--emit-static-lib", "a.o", "b.o", "-o", "libstuff.a"],
    compilerMode: {
      phase: cmd.CompilerPhase.Archive,
      artifact: cmd.CompilerArtifact.StaticLibrary,
    },
    inputs: ["a.o", "b.o"],
    outputs: ["libstuff.a"],
  },
  {
    label: "clang compile multiple translation units with default outputs",
    cmd: ["clang", "-c", "src/a.c", "src/b.cc"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/a.c", "src/b.cc"],
    outputs: ["a.o", "b.o"],
  },
  {
    label: "clang explicit windows msvc target object output",
    cmd: ["clang", "--target=x86_64-pc-windows-msvc", "-c", "main.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["main.c"],
    outputs: ["main.obj"],
    target: {
      artifactModel: cmd.CompilerArtifactModel.CoffMsvc,
      source: "argument",
      triple: "x86_64-pc-windows-msvc",
    },
  },
  {
    label: "prefixed clang windows msvc target",
    cmd: ["x86_64-pc-windows-msvc-clang", "-c", "main.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["main.c"],
    outputs: ["main.obj"],
    target: {
      artifactModel: cmd.CompilerArtifactModel.CoffMsvc,
      source: "executable-prefix",
      triple: "x86_64-pc-windows-msvc",
    },
  },
  {
    label: "explicit clang target overrides executable target",
    cmd: [
      "aarch64-linux-gnu-clang",
      "--target=x86_64-pc-windows-msvc",
      "-c",
      "main.c",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["main.c"],
    outputs: ["main.obj"],
    target: {
      artifactModel: cmd.CompilerArtifactModel.CoffMsvc,
      source: "argument",
      triple: "x86_64-pc-windows-msvc",
    },
  },
  {
    label: "versioned clang cross compiler target",
    cmd: ["aarch64-linux-gnu-clang-20", "-c", "main.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["main.c"],
    outputs: ["main.o"],
    target: {
      artifactModel: cmd.CompilerArtifactModel.Elf,
      source: "executable-prefix",
      triple: "aarch64-linux-gnu",
    },
  },
  {
    label: "clang unresolved windows gnu target executable output",
    cmd: ["clang", "--target=x86_64-w64-windows-gnu", "src/tool.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["src/tool.c"],
    outputs: [],
  },
  {
    label: "clang windows msvc target accepts coff link inputs",
    cmd: [
      "clang",
      "--target=x86_64-pc-windows-msvc",
      "obj/main.obj",
      "-o",
      "bin/app.exe",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["obj/main.obj"],
    outputs: ["bin/app.exe"],
  },
  {
    label: "clang linux target rejects coff link input suffix",
    cmd: [
      "clang",
      "--target=x86_64-unknown-linux-gnu",
      "obj/main.obj",
      "-o",
      "bin/app",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: [],
    outputs: ["bin/app"],
  },
  {
    label: "clang unresolved linux target executable output",
    cmd: ["clang", "--target=x86_64-unknown-linux-gnu", "src/tool.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["src/tool.c"],
    outputs: [],
  },
  {
    label: "prefixed mingw gnu driver object output",
    cmd: ["x86_64-w64-mingw32-g++", "-c", "main.cpp"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["main.cpp"],
    outputs: ["main.o"],
  },
  {
    label: "clang-cl cl-style compile no suffix into object dir",
    cmd: ["clang-cl", "/c", "/Tp", "src/noext", "/Fo:build/"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/noext"],
    outputs: [normalizedJoin("build", "noext.obj")],
  },
  {
    label: "clang-cl explicit linux target object output",
    cmd: ["clang-cl", "--target=x86_64-linux-gnu", "/c", "main.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["main.c"],
    outputs: ["main.o"],
    target: {
      artifactModel: cmd.CompilerArtifactModel.Elf,
      source: "argument",
      triple: "x86_64-linux-gnu",
    },
  },
  {
    label: "clang-cl cl-style default executable output",
    cmd: ["clang-cl", "src/main.cpp"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["src/main.cpp"],
    outputs: ["main.exe"],
    target: {
      artifactModel: cmd.CompilerArtifactModel.CoffMsvc,
      source: "driver-default",
      triple: "unknown-pc-windows-msvc",
    },
  },
  {
    label: "clang-cl dash preprocess to file mode",
    cmd: ["clang-cl", "-P", "src/main.cpp"],
    compilerMode: {
      phase: cmd.CompilerPhase.Preprocess,
      artifact: cmd.CompilerArtifact.PreprocessedSource,
    },
    inputs: ["src/main.cpp"],
    outputs: [],
  },
  {
    label: "clang-cl assembly listing does not stop link",
    cmd: ["clang-cl", "/FA", "src/main.cpp"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.Executable,
    },
    inputs: ["src/main.cpp"],
    outputs: ["main.exe", "main.asm"],
  },
  {
    label: "clang-cl assembly listing directory output",
    cmd: ["clang-cl", "/c", "/FA", "/Faasm/", "src/main.cpp"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/main.cpp"],
    outputs: ["main.obj", normalizedJoin("asm", "main.asm")],
  },
  {
    label: "clang-cl assembly listing single file output",
    cmd: ["clang-cl", "/c", "/Faasm.lst", "src/main.cpp"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/main.cpp"],
    outputs: ["main.obj", "asm.lst"],
  },
  {
    label: "clang-cl cl-style default shared library output",
    cmd: ["clang-cl", "/LD", "src/plugin.cpp"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.SharedLibrary,
    },
    inputs: ["src/plugin.cpp"],
    outputs: ["plugin.dll"],
  },
  {
    label: "msvc cl-style compile explicit object output",
    cmd: ["cl.exe", "/c", "src/main.cpp", "/Foobj/main.obj"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/main.cpp"],
    outputs: ["obj/main.obj"],
  },
  {
    label: "msvc cl-style shared output directory",
    cmd: ["cl.exe", "/LD", "src/plugin.cpp", "/Fe:bin/"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.SharedLibrary,
    },
    inputs: ["src/plugin.cpp"],
    outputs: [normalizedJoin("bin", "plugin.dll")],
  },
  {
    label: "msvc cl-style shared link via linker remainder",
    cmd: ["cl.exe", "/link", "/dll", "/out:bin/tool.dll", "foo.obj", "bar.res"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.SharedLibrary,
    },
    inputs: ["foo.obj", "bar.res"],
    outputs: ["bin/tool.dll"],
  },
  {
    label: "clang driver-mode cl compile no suffix into object dir",
    cmd: ["clang", "--driver-mode=cl", "/c", "/Tp", "src/noext", "/Fo:build/"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["src/noext"],
    outputs: [normalizedJoin("build", "noext.obj")],
  },
  {
    label: "clang driver-mode cl shared output directory",
    cmd: ["clang", "--driver-mode=cl", "/LD", "src/plugin.cpp", "/Fe:bin/"],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.SharedLibrary,
    },
    inputs: ["src/plugin.cpp"],
    outputs: [normalizedJoin("bin", "plugin.dll")],
  },
  {
    label: "clang driver-mode cl assembly listing keeps shared link mode",
    cmd: [
      "clang",
      "--driver-mode=cl",
      "/FA",
      "src/plugin.cpp",
      "/link",
      "/dll",
      "/out:bin/plugin.dll",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.SharedLibrary,
    },
    inputs: ["src/plugin.cpp"],
    outputs: ["bin/plugin.dll", "plugin.asm"],
  },
  {
    label: "clang driver-mode cl default object output",
    cmd: ["clang", "--driver-mode=cl", "-c", "main.c"],
    compilerMode: {
      phase: cmd.CompilerPhase.Compile,
      artifact: cmd.CompilerArtifact.Object,
    },
    inputs: ["main.c"],
    outputs: ["main.obj"],
  },
  {
    label: "clang driver-mode cl parses linker remainder after input",
    cmd: [
      "clang",
      "--driver-mode=cl",
      "foo.obj",
      "/link",
      "/dll",
      "/out:bin/tool.dll",
      "bar.res",
    ],
    compilerMode: {
      phase: cmd.CompilerPhase.Link,
      artifact: cmd.CompilerArtifact.SharedLibrary,
    },
    inputs: ["foo.obj", "bar.res"],
    outputs: ["bin/tool.dll"],
  },
];

for (const testCase of cases) {
  expectAnalysis(testCase);
}

expectAnalysisError(
  compilerAnalyzer.analyze(invocation(["clang-cl", "-S", "src/main.cpp"])),
  "clang-cl gnu assembly stop action",
);
expectAnalysisError(
  compilerAnalyzer.analyze(
    invocation(["clang-cl", "-emit-llvm", "-c", "src/main.cpp"]),
  ),
  "clang-cl gnu llvm artifact action",
);

compilerIdentifier.registerCompilerRule("test:cross-gcc", {
  dialect: cmd.CompilerDialect.Gcc,
  match: [/^my-cross-tool$/, /^\/opt\/bin\/my-cross-tool$/],
  target: { triple: "x86_64-pc-windows-msvc" },
});
expectAnalysis({
  label: "custom gnu compiler rule",
  cmd: ["my-cross-tool", "-c", "src/custom.c"],
  compilerMode: {
    phase: cmd.CompilerPhase.Compile,
    artifact: cmd.CompilerArtifact.Object,
  },
  inputs: ["src/custom.c"],
  outputs: ["custom.obj"],
  target: {
    artifactModel: cmd.CompilerArtifactModel.CoffMsvc,
    source: "compiler-rule",
    triple: "x86_64-pc-windows-msvc",
  },
});
const customAbsoluteExeAnalysis = expectCompilerAnalysis(
  compilerAnalyzer.analyze(
    invocation(
      ["my-cross-tool", "-c", "src/custom-absolute.c"],
      "/opt/bin/my-cross-tool",
    ),
  ),
  "custom absolute executable compiler analysis",
);
expectArrayEq(
  customAbsoluteExeAnalysis.writes,
  ["custom-absolute.obj"],
  "custom absolute executable writes",
);

compilerIdentifier.registerCompilerRule("test:cross-gcc", {
  dialect: cmd.CompilerDialect.Clang,
  match: /^my-cross-tool$/,
});
expectAnalysis({
  label: "custom compiler rule replacement",
  cmd: ["my-cross-tool", "-c", "src/custom.c"],
  compilerMode: {
    phase: cmd.CompilerPhase.Compile,
    artifact: cmd.CompilerArtifact.Object,
  },
  inputs: ["src/custom.c"],
  outputs: ["custom.o"],
});
compilerIdentifier.unregisterCompilerRule("test:cross-gcc");
debug.assertThrow(
  expectAnalysisError(
    compilerAnalyzer.analyze(
      invocation(["my-cross-tool", "-c", "src/custom.c"]),
    ),
    "unregistered custom compiler",
  ) instanceof cmd.CompilerUnsupportedError,
);
