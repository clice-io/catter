import {
  debug,
  service,
  type CatterConfig,
  type CatterRuntime,
  type CommandCaptureResult,
} from "catter";

const runtimeInfo: CatterRuntime = {
  supportActions: ["skip", "drop", "abort", "modify"],
  type: "inject",
  supportParentId: true,
};

const config: CatterConfig = {
  scriptPath: "runtime-test.ts",
  scriptArgs: ["--input"],
  buildSystemCommand: ["make"],
  buildSystemCommandCwd: "/tmp",
  runtime: runtimeInfo,
  options: {
    log: true,
  },
  execute: true,
};

function command(exe: string, parent?: number): CommandCaptureResult {
  return {
    success: true,
    data: {
      cwd: "/tmp",
      exe,
      argv: [exe],
      env: [],
      runtime: runtimeInfo,
      parent,
    },
  };
}

const commandIds: number[] = [];
const executionIds: number[] = [];

const runtime = new service.ServiceRuntime();
runtime.use(
  service.create({
    async onStart(startConfig) {
      await Promise.resolve();
      return {
        ...startConfig,
        scriptArgs: [...startConfig.scriptArgs, "--from-runtime"],
      };
    },

    async onCommand(ctx) {
      await Promise.resolve();
      commandIds.push(ctx.id);

      if (ctx.capture.success && ctx.capture.data.exe === "gcc") {
        ctx.ignoreDescendants();
        ctx.modify({
          ...ctx.capture.data,
          argv: [...ctx.capture.data.argv, "-Wall"],
        });
      }
    },

    async onExecution(ctx) {
      await Promise.resolve();
      executionIds.push(ctx.id);
    },
  }),
);

const updatedConfig = await runtime.start(config);
debug.assertThrow(
  updatedConfig.scriptArgs[updatedConfig.scriptArgs.length - 1] ===
    "--from-runtime",
);

const rootAction = await runtime.command(1, command("gcc"));
debug.assertThrow(rootAction.type === "modify");
if (rootAction.type === "modify") {
  debug.assertThrow(
    rootAction.data.argv[rootAction.data.argv.length - 1] === "-Wall",
  );
}

const childAction = await runtime.command(2, command("cc1", 1));
debug.assertThrow(childAction.type === "skip");
debug.assertThrow(commandIds.length === 1);
debug.assertThrow(commandIds[0] === 1);
debug.assertThrow(runtime.hasCommand(2));
debug.assertThrow(runtime.hasIgnoredAncestor(2));

await runtime.execution(1, { code: 0, stdout: "", stderr: "" });
await runtime.execution(2, { code: 0, stdout: "", stderr: "" });
debug.assertThrow(executionIds.length === 1);
debug.assertThrow(executionIds[0] === 1);

const parallelStartRuntime = new service.ServiceRuntime();
parallelStartRuntime.use(
  service.parallel(
    service.create({
      onStart(startConfig) {
        return {
          ...startConfig,
          scriptArgs: [...startConfig.scriptArgs, "--parallel"],
        };
      },
    }),
    service.create({
      async onStart(startConfig) {
        await Promise.resolve();
        return {
          ...startConfig,
          scriptArgs: [...startConfig.scriptArgs, "--parallel"],
        };
      },
    }),
    service.create({
      onStart() {},
    }),
  ),
);

const parallelStartedConfig = await parallelStartRuntime.start(config);
debug.assertThrow(
  parallelStartedConfig.scriptArgs[
    parallelStartedConfig.scriptArgs.length - 1
  ] === "--parallel",
);

const parallelStartConflictRuntime = new service.ServiceRuntime();
parallelStartConflictRuntime.use(
  service.parallel(
    service.create({
      onStart(startConfig) {
        return {
          ...startConfig,
          execute: false,
        };
      },
    }),
    service.create({
      onStart(startConfig) {
        return {
          ...startConfig,
          execute: true,
        };
      },
    }),
  ),
);

let parallelStartConflictSeen = false;
try {
  await parallelStartConflictRuntime.start(config);
} catch (error) {
  parallelStartConflictSeen = String(error).includes(
    "identical onStart results",
  );
}
debug.assertThrow(parallelStartConflictSeen);

const pipelineEvents: string[] = [];
const pipelineRuntime = new service.ServiceRuntime();
pipelineRuntime.use(
  service.parallel(
    service.create({
      async onCommand(ctx) {
        await Promise.resolve();
        pipelineEvents.push(`seen:${ctx.id}`);
        if (ctx.capture.success && ctx.capture.data.exe === "clang") {
          ctx.ignoreDescendants();
        }
      },
    }),
    {
      async onCommand(id, data) {
        await Promise.resolve();
        pipelineEvents.push(`action:${id}`);
        if (data.success && data.data.exe === "clang") {
          return {
            type: "modify",
            data: {
              ...data.data,
              argv: [...data.data.argv, "-O2"],
            },
          };
        }
        return { type: "skip" };
      },
    },
    service.create({
      async onExecution(ctx) {
        await Promise.resolve();
        pipelineEvents.push(`exec:${ctx.id}`);
      },
    }),
  ),
);

const sequentialPipelineRuntime = new service.ServiceRuntime();
const sequentialPipelineEvents: string[] = [];
sequentialPipelineRuntime.use(
  service.pipeline(
    service.create({
      onCommand(ctx) {
        sequentialPipelineEvents.push("first");
        if (!ctx.capture.success) {
          return;
        }
        ctx.modify({
          ...ctx.capture.data,
          argv: [...ctx.capture.data.argv, "first"],
        });
      },
    }),
    service.create({
      onCommand(ctx) {
        sequentialPipelineEvents.push(`second:${ctx.action.type}`);
        if (!ctx.capture.success) {
          return;
        }
        ctx.modify({
          ...ctx.capture.data,
          argv: [...ctx.capture.data.argv, "second"],
        });
      },
    }),
  ),
);

const sequentialPipelineAction = await sequentialPipelineRuntime.command(
  30,
  command("clang++"),
);
debug.assertThrow(sequentialPipelineAction.type === "modify");
if (sequentialPipelineAction.type === "modify") {
  debug.assertThrow(
    sequentialPipelineAction.data.argv[
      sequentialPipelineAction.data.argv.length - 1
    ] === "second",
  );
}
debug.assertThrow(sequentialPipelineEvents.join(",") === "first,second:modify");

const pipelineAction = await pipelineRuntime.command(10, command("clang"));
debug.assertThrow(pipelineAction.type === "modify");
if (pipelineAction.type === "modify") {
  debug.assertThrow(
    pipelineAction.data.argv[pipelineAction.data.argv.length - 1] === "-O2",
  );
}
debug.assertThrow(pipelineEvents.includes("seen:10"));
debug.assertThrow(pipelineEvents.includes("action:10"));

const pipelineChildAction = await pipelineRuntime.command(
  11,
  command("cc1", 10),
);
debug.assertThrow(pipelineChildAction.type === "skip");
debug.assertThrow(!pipelineEvents.includes("seen:11"));
debug.assertThrow(!pipelineEvents.includes("action:11"));

await pipelineRuntime.execution(10, { code: 0, stdout: "", stderr: "" });
await pipelineRuntime.execution(11, { code: 0, stdout: "", stderr: "" });
debug.assertThrow(pipelineEvents.includes("exec:10"));
debug.assertThrow(!pipelineEvents.includes("exec:11"));

const conflictRuntime = new service.ServiceRuntime();
conflictRuntime.use(
  service.parallel(
    service.create({
      onCommand(ctx) {
        ctx.drop();
      },
    }),
    {
      onCommand() {
        return { type: "skip" } as const;
      },
    },
  ),
);

let conflictSeen = false;
try {
  await conflictRuntime.command(20, command("ld"));
} catch (error) {
  conflictSeen = String(error).includes("at most one action result");
}
debug.assertThrow(conflictSeen);
