import {
  cmd,
  debug,
  fs,
  scripts,
  service,
  type CatterConfig,
  type CatterRuntime,
  type CommandCaptureResult,
} from "catter";

const runtime: CatterRuntime = {
  supportActions: ["skip", "drop", "abort", "modify"],
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

function captureError(message: string): CommandCaptureResult {
  return {
    success: false,
    error: {
      msg: message,
    },
  };
}

function config(scriptArgs: string[]): CatterConfig {
  return {
    scriptPath: "cdb-test.ts",
    scriptArgs,
    buildSystemCommand: ["make"],
    buildSystemCommandCwd: fs.path.absolute(testEnvPath),
    runtime,
    options: {
      log: false,
    },
    execute: true,
  };
}

const testEnvPath = fs.path.joinAll(
  ".",
  `cdb-script-test-env-${Date.now()}-${Math.floor(Math.random() * 1_000_000)}`,
);

try {
  debug.assertThrow(fs.mkdir(testEnvPath));

  const savePath = fs.path.joinAll(testEnvPath, "compile_commands.json");
  const serviceRuntime = new service.ServiceRuntime();
  serviceRuntime.use(scripts.cdb(savePath));

  expectSkip(
    await serviceRuntime.command(
      1,
      command(["make"], fs.path.absolute(testEnvPath)),
    ),
  );
  expectSkip(
    await serviceRuntime.command(
      2,
      command(
        ["clang++", "-c", "src/main.cc", "-o", "obj/main.o"],
        fs.path.absolute(testEnvPath),
        1,
      ),
    ),
  );
  expectSkip(
    await serviceRuntime.command(
      3,
      command(
        ["clang++", "obj/main.o", "-o", "bin/app"],
        fs.path.absolute(testEnvPath),
        1,
      ),
    ),
  );

  await serviceRuntime.finish({ code: 0, stdout: "", stderr: "" });

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

  const failedSavePath = fs.path.joinAll(testEnvPath, "failed.json");
  const failedRuntime = new service.ServiceRuntime();
  failedRuntime.use(scripts.cdb());
  await failedRuntime.start(
    config(["--output", failedSavePath, "--save-on-failure", "--quiet"]),
  );
  expectSkip(
    await failedRuntime.command(
      10,
      command(
        ["clang++", "-c", "src/failed.cc", "-o", "obj/failed.o"],
        fs.path.absolute(testEnvPath),
      ),
    ),
  );
  await failedRuntime.finish({ code: 1, stdout: "", stderr: "" });
  debug.assertThrow(new cmd.CDBManager(failedSavePath).items().length === 1);

  const skippedFailurePath = fs.path.joinAll(
    testEnvPath,
    "skipped-failure.json",
  );
  const skippedFailureRuntime = new service.ServiceRuntime();
  skippedFailureRuntime.use(scripts.cdb());
  await skippedFailureRuntime.start(
    config(["--output", skippedFailurePath, "--quiet"]),
  );
  expectSkip(
    await skippedFailureRuntime.command(
      11,
      command(
        ["clang++", "-c", "src/skipped.cc", "-o", "obj/skipped.o"],
        fs.path.absolute(testEnvPath),
      ),
    ),
  );
  await skippedFailureRuntime.finish({ code: 1, stdout: "", stderr: "" });
  debug.assertThrow(!fs.exists(skippedFailurePath));

  const appendPath = fs.path.joinAll(testEnvPath, "append.json");
  const inherited = new cmd.CDBManager(appendPath, { inherit: false });
  inherited.addItem({
    directory: fs.path.absolute(testEnvPath),
    file: "src/inherited.cc",
    arguments: ["clang++", "-c", "src/inherited.cc"],
  });
  inherited.save();

  const appendRuntime = new service.ServiceRuntime();
  appendRuntime.use(scripts.cdb());
  await appendRuntime.start(config(["--output", appendPath, "--quiet"]));
  expectSkip(
    await appendRuntime.command(
      12,
      command(
        ["clang++", "-c", "src/appended.cc", "-o", "obj/appended.o"],
        fs.path.absolute(testEnvPath),
      ),
    ),
  );
  await appendRuntime.finish({ code: 0, stdout: "", stderr: "" });
  const appendedItems = new cmd.CDBManager(appendPath).items();
  debug.assertThrow(appendedItems.length === 2);
  debug.assertThrow(
    appendedItems.some((item) => item.file === "src/inherited.cc"),
  );
  debug.assertThrow(
    appendedItems.some((item) => item.file === "src/appended.cc"),
  );

  const replacePath = fs.path.joinAll(testEnvPath, "replace.json");
  const replaceInherited = new cmd.CDBManager(replacePath, { inherit: false });
  replaceInherited.addItem({
    directory: fs.path.absolute(testEnvPath),
    file: "src/old.cc",
    arguments: ["clang++", "-c", "src/old.cc"],
  });
  replaceInherited.save();

  const replaceRuntime = new service.ServiceRuntime();
  replaceRuntime.use(scripts.cdb());
  await replaceRuntime.start(
    config(["--output", replacePath, "--replace", "--quiet"]),
  );
  expectSkip(
    await replaceRuntime.command(
      15,
      command(
        ["clang++", "-c", "src/new.cc", "-o", "obj/new.o"],
        fs.path.absolute(testEnvPath),
      ),
    ),
  );
  await replaceRuntime.finish({ code: 0, stdout: "", stderr: "" });
  const replacedItems = new cmd.CDBManager(replacePath).items();
  debug.assertThrow(replacedItems.length === 1);
  debug.assertThrow(replacedItems[0]?.file === "src/new.cc");

  const abortCommandPath = fs.path.joinAll(testEnvPath, "abort-command.json");
  const abortCommandRuntime = new service.ServiceRuntime();
  abortCommandRuntime.use(scripts.cdb());
  await abortCommandRuntime.start(
    config([
      "--output",
      abortCommandPath,
      "--abort-on-command-failure",
      "--save-on-failure",
      "--quiet",
    ]),
  );
  expectSkip(
    await abortCommandRuntime.command(
      13,
      command(
        ["clang++", "-c", "src/broken.cc", "-o", "obj/broken.o"],
        fs.path.absolute(testEnvPath),
      ),
    ),
  );

  let commandFailureAborted = false;
  try {
    await abortCommandRuntime.execution(13, {
      code: 2,
      stdout: "",
      stderr: "",
    });
  } catch (error) {
    commandFailureAborted = String(error).includes("exited with code 2");
  }
  debug.assertThrow(commandFailureAborted);
  debug.assertThrow(new cmd.CDBManager(abortCommandPath).items().length === 1);

  const abortCaptureRuntime = new service.ServiceRuntime();
  abortCaptureRuntime.use(scripts.cdb());
  await abortCaptureRuntime.start(
    config(["--abort-on-capture-error", "--quiet"]),
  );
  let captureErrorAborted = false;
  try {
    await abortCaptureRuntime.command(14, captureError("spawn failed"));
  } catch (error) {
    captureErrorAborted = String(error).includes("spawn failed");
  }
  debug.assertThrow(captureErrorAborted);
} finally {
  if (fs.exists(testEnvPath)) {
    fs.removeAll(testEnvPath);
  }
}

function expectSkip(action: { type: string }): void {
  debug.assertThrow(action.type === "skip");
}
