import { cmd, debug, fs, os } from "catter";

type ExpectedAnalysis = {
  label: string;
  cmd: string[];
  compiler: "clang" | "gcc";
  phase: (typeof cmd.CompilerPhase)[keyof typeof cmd.CompilerPhase];
  artifact: (typeof cmd.CompilerArtifact)[keyof typeof cmd.CompilerArtifact];
  type:
    | (typeof cmd.CompilerCommandType)[keyof typeof cmd.CompilerCommandType]
    | undefined;
  inputs: string[];
  outputs: string[];
  cdbEntries: Array<{
    file: string;
    output?: string;
  }>;
};

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function expectArrayEq(actual: string[], expected: string[], label: string) {
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

function expectCDBEntriesEq(
  actual: Array<{ file: string; output?: string }>,
  expected: Array<{ file: string; output?: string }>,
  label: string,
) {
  if (actual.length !== expected.length) {
    throw new Error(
      `${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`,
    );
  }

  for (let idx = 0; idx < actual.length; ++idx) {
    if (
      actual[idx].file !== expected[idx].file ||
      actual[idx].output !== expected[idx].output
    ) {
      throw new Error(
        `${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`,
      );
    }
  }
}

function normalizedJoin(...parts: string[]) {
  return fs.path.lexicalNormal(fs.path.joinAll(...parts));
}

function expectAnalysis(expected: ExpectedAnalysis) {
  const analysis = new cmd.CompilerCmdAnalysis(expected.cmd);

  expectEq(analysis.compiler, expected.compiler, `${expected.label} compiler`);
  expectEq(analysis.phase, expected.phase, `${expected.label} phase`);
  expectEq(analysis.artifact, expected.artifact, `${expected.label} artifact`);
  expectEq(analysis.type, expected.type, `${expected.label} legacy type`);
  expectArrayEq(analysis.inputs(), expected.inputs, `${expected.label} inputs`);
  expectArrayEq(
    analysis.outputs(),
    expected.outputs,
    `${expected.label} outputs`,
  );
  expectCDBEntriesEq(
    analysis.compilationDatabaseEntries(),
    expected.cdbEntries,
    `${expected.label} cdb entries`,
  );
}

debug.assertThrow(
  cmd.CompilerCmdAnalysis.isSupport(["clang", "-c", "main.cc"]),
);
debug.assertThrow(cmd.CompilerCmdAnalysis.isSupport(["gcc", "-c", "main.cc"]));
debug.assertThrow(
  !cmd.CompilerCmdAnalysis.isSupport(["nvcc", "-c", "kernel.cu"]),
);

const cases: ExpectedAnalysis[] = [
  {
    label: "clang llvm ir explicit stdout output",
    cmd: ["clang", "src/t.c", "-S", "-emit-llvm", "-o", "-"],
    compiler: "clang",
    phase: cmd.CompilerPhase.Compile,
    artifact: cmd.CompilerArtifact.LlvmIR,
    type: cmd.CompilerCommandType.SourceToLlvmIR,
    inputs: ["src/t.c"],
    outputs: ["-"],
    cdbEntries: [],
  },
  {
    label: "gcc preprocess explicit language without suffix",
    cmd: ["gcc", "-x", "c", "generated_input", "-E", "-P"],
    compiler: "gcc",
    phase: cmd.CompilerPhase.Preprocess,
    artifact: cmd.CompilerArtifact.Stdout,
    type: cmd.CompilerCommandType.SourcePreprocess,
    inputs: ["generated_input"],
    outputs: [],
    cdbEntries: [],
  },
  {
    label: "gcc syntax-only explicit language",
    cmd: ["gcc", "-x", "c++", "generated", "-fsyntax-only", "-fno-exceptions"],
    compiler: "gcc",
    phase: cmd.CompilerPhase.SyntaxOnly,
    artifact: cmd.CompilerArtifact.None,
    type: cmd.CompilerCommandType.SourceSyntaxOnly,
    inputs: ["generated"],
    outputs: [],
    cdbEntries: [],
  },
  {
    label: "gcc x-none resets classification for later object inputs",
    cmd: [
      "gcc",
      "-x",
      "c",
      "generated_input",
      "-x",
      "none",
      "obj/plain.o",
      "-o",
      "bin/app",
    ],
    compiler: "gcc",
    phase: cmd.CompilerPhase.Link,
    artifact: cmd.CompilerArtifact.Executable,
    type: cmd.CompilerCommandType.SourceToExe,
    inputs: ["generated_input", "obj/plain.o"],
    outputs: ["bin/app"],
    cdbEntries: [],
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
    compiler: "gcc",
    phase: cmd.CompilerPhase.RelocatableLink,
    artifact: cmd.CompilerArtifact.Object,
    type: cmd.CompilerCommandType.RelocatableLink,
    inputs: ["a.o", "b.o"],
    outputs: ["partial.o"],
    cdbEntries: [],
  },
  {
    label: "clang archive static lib from object inputs",
    cmd: ["clang", "--emit-static-lib", "a.o", "b.o", "-o", "libstuff.a"],
    compiler: "clang",
    phase: cmd.CompilerPhase.Archive,
    artifact: cmd.CompilerArtifact.StaticLibrary,
    type: cmd.CompilerCommandType.ObjectToLib,
    inputs: ["a.o", "b.o"],
    outputs: ["libstuff.a"],
    cdbEntries: [],
  },
  {
    label: "clang cl-style shared link via linker remainder",
    cmd: [
      "clang",
      "--driver-mode=cl",
      "/link",
      "/dll",
      "/out:bin/tool.dll",
      "foo.obj",
      "bar.res",
    ],
    compiler: "clang",
    phase: cmd.CompilerPhase.Link,
    artifact: cmd.CompilerArtifact.SharedLibrary,
    type: cmd.CompilerCommandType.ObjectToShare,
    inputs: ["foo.obj", "bar.res"],
    outputs: ["bin/tool.dll"],
    cdbEntries: [],
  },
  {
    label: "clang compile multiple translation units with default outputs",
    cmd: ["clang", "-c", "src/a.c", "src/b.cc"],
    compiler: "clang",
    phase: cmd.CompilerPhase.Compile,
    artifact: cmd.CompilerArtifact.Object,
    type: cmd.CompilerCommandType.SourceToObject,
    inputs: ["src/a.c", "src/b.cc"],
    outputs: ["a.o", "b.o"],
    cdbEntries: [
      {
        file: "src/a.c",
        output: "a.o",
      },
      {
        file: "src/b.cc",
        output: "b.o",
      },
    ],
  },
];

if (os.platform() === "windows") {
  cases.splice(1, 0, {
    label: "clang cl-style compile no suffix into object dir",
    cmd: [
      "clang",
      "--driver-mode=cl",
      "/c",
      "/Tp",
      "src/noext",
      "/Fo",
      "build/",
    ],
    compiler: "clang",
    phase: cmd.CompilerPhase.Compile,
    artifact: cmd.CompilerArtifact.Object,
    type: cmd.CompilerCommandType.SourceToObject,
    inputs: ["src/noext"],
    outputs: [normalizedJoin("build", "noext.obj")],
    cdbEntries: [
      {
        file: "src/noext",
        output: normalizedJoin("build", "noext.obj"),
      },
    ],
  });
} else {
  const clStyleSuppressed = new cmd.CompilerCmdAnalysis([
    "clang",
    "--driver-mode=cl",
    "main.c",
  ]);
  expectEq(
    clStyleSuppressed.style,
    "gnu",
    "clang cl-style visibility suppressed style",
  );
}

for (const testCase of cases) {
  expectAnalysis(testCase);
}
