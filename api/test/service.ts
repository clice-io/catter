import { debug, service } from "catter";

const serviceArg = "--from-service";

let outputEventSeen = false;
let commandErrorBranchSeen = false;

service.register({
  async onStart(config) {
    await Promise.resolve();
    debug.assertThrow(config.scriptPath === "script.ts");
    debug.assertThrow(config.scriptArgs.length === 2);
    debug.assertThrow(config.options.log);

    return {
      ...config,
      scriptArgs: [...config.scriptArgs, serviceArg],
      options: {
        ...config.options,
        log: false,
      },
      execute: true,
    };
  },

  async onFinish(event) {
    await Promise.resolve();
    debug.assertThrow(outputEventSeen);
    debug.assertThrow(commandErrorBranchSeen);
    debug.assertThrow(event.code === 0);
    debug.assertThrow(event.stdout === "");
    debug.assertThrow(event.stderr === "");
  },

  async onCommand(id, data) {
    await Promise.resolve();
    debug.assertThrow(id === 7);

    if (!data.success) {
      debug.assertThrow(data.error.msg === "spawn failed");
      commandErrorBranchSeen = true;
      return { type: "skip" };
    }

    debug.assertThrow(data.data.cwd === "/tmp");
    debug.assertThrow(data.data.exe === "clang++");
    debug.assertThrow(data.data.argv.length === 3);
    debug.assertThrow(data.data.argv[2] === "-c");
    debug.assertThrow(data.data.parent === 41);

    return {
      type: "modify",
      data: {
        ...data.data,
        argv: [...data.data.argv, serviceArg],
      },
    };
  },

  async onExecution(id, event) {
    await Promise.resolve();
    debug.assertThrow(id === 7);
    debug.assertThrow(event.code === 0);
    debug.assertThrow(event.stdout === "hello from stdout");
    debug.assertThrow(event.stderr === "hello from stderr");
    outputEventSeen = true;
  },
});
