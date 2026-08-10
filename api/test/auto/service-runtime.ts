import { assertThrow } from "catter/debug";
import { ServiceRuntime, create, parallel, pipeline } from "catter/service";
import { ok, type Result } from "catter/neverthrow";
import type {
  CatterConfig,
  CatterErr,
  CatterRuntime,
  CommandData,
} from "catter/service";

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
    stdioMode: "capture",
  },
  execute: true,
};

function command(exe: string, parent?: number): Result<CommandData, CatterErr> {
  return ok({
    cwd: "/tmp",
    exe,
    argv: [exe],
    env: [],
    runtime: runtimeInfo,
    parent,
  });
}

const commandIds: number[] = [];
const executionIds: number[] = [];

const runtime = new ServiceRuntime();
runtime.use(
  create({
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

      if (ctx.capture.isOk() && ctx.capture.value.exe === "gcc") {
        ctx.ignoreDescendants();
        ctx.modify({
          ...ctx.capture.value,
          argv: [...ctx.capture.value.argv, "-Wall"],
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
assertThrow(
  updatedConfig.scriptArgs[updatedConfig.scriptArgs.length - 1] ===
    "--from-runtime",
);

const rootAction = await runtime.command(1, command("gcc"));
assertThrow(rootAction.type === "modify");
if (rootAction.type === "modify") {
  assertThrow(
    rootAction.data.argv[rootAction.data.argv.length - 1] === "-Wall",
  );
}

const childAction = await runtime.command(2, command("cc1", 1));
assertThrow(childAction.type === "skip");
assertThrow(commandIds.length === 1);
assertThrow(commandIds[0] === 1);
assertThrow(runtime.hasCommand(2));
assertThrow(runtime.hasIgnoredAncestor(2));

await runtime.execution(1, { code: 0, stdout: "", stderr: "" });
await runtime.execution(2, { code: 0, stdout: "", stderr: "" });
assertThrow(executionIds.length === 1);
assertThrow(executionIds[0] === 1);

const parallelStartRuntime = new ServiceRuntime();
parallelStartRuntime.use(
  parallel(
    create({
      onStart(startConfig) {
        return {
          ...startConfig,
          scriptArgs: [...startConfig.scriptArgs, "--parallel"],
        };
      },
    }),
    create({
      async onStart(startConfig) {
        await Promise.resolve();
        return {
          ...startConfig,
          scriptArgs: [...startConfig.scriptArgs, "--parallel"],
        };
      },
    }),
    create({
      onStart() {},
    }),
  ),
);

const parallelStartedConfig = await parallelStartRuntime.start(config);
assertThrow(
  parallelStartedConfig.scriptArgs[
    parallelStartedConfig.scriptArgs.length - 1
  ] === "--parallel",
);

const parallelStartConflictRuntime = new ServiceRuntime();
parallelStartConflictRuntime.use(
  parallel(
    create({
      onStart(startConfig) {
        return {
          ...startConfig,
          execute: false,
        };
      },
    }),
    create({
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
assertThrow(parallelStartConflictSeen);

const pipelineEvents: string[] = [];
const pipelineRuntime = new ServiceRuntime();
pipelineRuntime.use(
  parallel(
    create({
      async onCommand(ctx) {
        await Promise.resolve();
        pipelineEvents.push(`seen:${ctx.id}`);
        if (ctx.capture.isOk() && ctx.capture.value.exe === "clang") {
          ctx.ignoreDescendants();
        }
      },
    }),
    {
      async onCommand(id, data) {
        await Promise.resolve();
        pipelineEvents.push(`action:${id}`);
        if (data.isOk() && data.value.exe === "clang") {
          return {
            type: "modify",
            data: {
              ...data.value,
              argv: [...data.value.argv, "-O2"],
            },
          };
        }
        return { type: "skip" };
      },
    },
    create({
      async onExecution(ctx) {
        await Promise.resolve();
        pipelineEvents.push(`exec:${ctx.id}`);
      },
    }),
  ),
);

const sequentialPipelineRuntime = new ServiceRuntime();
const sequentialPipelineEvents: string[] = [];
sequentialPipelineRuntime.use(
  pipeline(
    create({
      onCommand(ctx) {
        sequentialPipelineEvents.push("first");
        if (ctx.capture.isErr()) {
          return;
        }
        ctx.modify({
          ...ctx.capture.value,
          argv: [...ctx.capture.value.argv, "first"],
        });
      },
    }),
    create({
      onCommand(ctx) {
        sequentialPipelineEvents.push(`second:${ctx.action.type}`);
        if (ctx.capture.isErr()) {
          return;
        }
        ctx.modify({
          ...ctx.capture.value,
          argv: [...ctx.capture.value.argv, "second"],
        });
      },
    }),
  ),
);

const sequentialPipelineAction = await sequentialPipelineRuntime.command(
  30,
  command("clang++"),
);
assertThrow(sequentialPipelineAction.type === "modify");
if (sequentialPipelineAction.type === "modify") {
  assertThrow(
    sequentialPipelineAction.data.argv[
      sequentialPipelineAction.data.argv.length - 1
    ] === "second",
  );
}
assertThrow(sequentialPipelineEvents.join(",") === "first,second:modify");

const pipelineAction = await pipelineRuntime.command(10, command("clang"));
assertThrow(pipelineAction.type === "modify");
if (pipelineAction.type === "modify") {
  assertThrow(
    pipelineAction.data.argv[pipelineAction.data.argv.length - 1] === "-O2",
  );
}
assertThrow(pipelineEvents.includes("seen:10"));
assertThrow(pipelineEvents.includes("action:10"));

const pipelineChildAction = await pipelineRuntime.command(
  11,
  command("cc1", 10),
);
assertThrow(pipelineChildAction.type === "skip");
assertThrow(!pipelineEvents.includes("seen:11"));
assertThrow(!pipelineEvents.includes("action:11"));

await pipelineRuntime.execution(10, { code: 0, stdout: "", stderr: "" });
await pipelineRuntime.execution(11, { code: 0, stdout: "", stderr: "" });
assertThrow(pipelineEvents.includes("exec:10"));
assertThrow(!pipelineEvents.includes("exec:11"));

const cyclicParentRuntime = new ServiceRuntime();
cyclicParentRuntime.use(
  create({
    onCommand(ctx) {
      if (ctx.capture.isErr()) {
        return;
      }
      ctx.skip();
    },
  }),
);

// 40 reports 41 as its parent while 41 reports 40, forming a parent cycle.
// Walking the ancestor chain must terminate instead of looping forever.
const cyclicParentA = await cyclicParentRuntime.command(40, command("cc1", 41));
const cyclicParentB = await cyclicParentRuntime.command(41, command("cc1", 40));
assertThrow(cyclicParentA.type === "skip");
assertThrow(cyclicParentB.type === "skip");
assertThrow(!cyclicParentRuntime.hasIgnoredAncestor(40));
assertThrow(!cyclicParentRuntime.hasIgnoredAncestor(41));

const conflictRuntime = new ServiceRuntime();
conflictRuntime.use(
  parallel(
    create({
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
assertThrow(conflictSeen);
