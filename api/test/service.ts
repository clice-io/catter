import * as debug from "catter/debug";
import * as service from "catter/service";

const serviceArg = "--from-service";

let outputEventSeen = false;
let commandErrorBranchSeen = false;

service.register({
  async onStart(config) {
    await Promise.resolve();
    debug.assertThrow(config.scriptPath === "script.ts");
    debug.assertThrow(config.scriptArgs.length === 2);
    debug.assertThrow(config.options.log);
    debug.assertThrow(config.options.stdioMode === "inherit");

    return {
      ...config,
      scriptArgs: [...config.scriptArgs, serviceArg],
      options: {
        ...config.options,
        log: false,
        stdioMode: "capture",
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

    if (data.isErr()) {
      debug.assertThrow(data.error.msg === "spawn failed");
      commandErrorBranchSeen = true;
      return { type: "skip" };
    }

    debug.assertThrow(data.value.cwd === "/tmp");
    debug.assertThrow(data.value.exe === "clang++");
    debug.assertThrow(data.value.argv.length === 3);
    debug.assertThrow(data.value.argv[2] === "-c");
    debug.assertThrow(data.value.parent === 41);

    return {
      type: "modify",
      data: {
        ...data.value,
        argv: [...data.value.argv, serviceArg],
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
