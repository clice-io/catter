import {
  cmd,
  debug,
  fs,
  scripts,
  type CatterRuntime,
  type CommandCaptureResult,
} from "catter";

const runtime: CatterRuntime = {
  supportActions: ["skip", "drop", "abort", "modify"],
  supportEvents: ["finish"],
  type: "inject",
  supportParentId: true,
};

function command(
  argv: string[],
  cwd: string,
  parent?: number,
): CommandCaptureResult {
  return {
    success: true,
    data: {
      cwd,
      exe: argv[0]!,
      argv,
      env: [],
      runtime,
      parent,
    },
  };
}

const testEnvPath = fs.path.joinAll(
  ".",
  `cdb-script-test-env-${Date.now()}-${Math.floor(Math.random() * 1_000_000)}`,
);

try {
  debug.assertThrow(fs.mkdir(testEnvPath));

  const savePath = fs.path.joinAll(testEnvPath, "compile_commands.json");
  const serviceView = new scripts.CDB(savePath).asService();

  expectSkip(
    serviceView.onCommand(1, command(["make"], fs.path.absolute(testEnvPath))),
  );
  expectSkip(
    serviceView.onCommand(
      2,
      command(
        ["clang++", "-c", "src/main.cc", "-o", "obj/main.o"],
        fs.path.absolute(testEnvPath),
        1,
      ),
    ),
  );
  expectSkip(
    serviceView.onCommand(
      3,
      command(
        ["clang++", "obj/main.o", "-o", "bin/app"],
        fs.path.absolute(testEnvPath),
        1,
      ),
    ),
  );

  serviceView.onFinish({ type: "finish", code: 0 });

  const items = new cmd.CDBManager(savePath).items();
  debug.assertThrow(items.length === 1);
  debug.assertThrow(items[0] !== undefined);
  if (items[0] === undefined) {
    throw new Error("expected a saved cdb item");
  }
  debug.assertThrow(items[0].file === "src/main.cc");
  debug.assertThrow(
    items[0].output ===
      fs.path.lexicalNormal(
        fs.path.absolute(fs.path.joinAll(testEnvPath, "obj", "main.o")),
      ),
  );
} finally {
  if (fs.exists(testEnvPath)) {
    fs.removeAll(testEnvPath);
  }
}

function expectSkip(action: { type: string }): void {
  debug.assertThrow(action.type === "skip");
}
