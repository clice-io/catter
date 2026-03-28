import { debug, service } from "catter";

const serviceArg = "--from-service";

let outputEventSeen = false;
let finishEventSeen = false;
let commandErrorBranchSeen = false;

service.register({
  onStart(config) {
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
      execute: false,
    };
  },

  onFinish(event) {
    debug.assertThrow(outputEventSeen);
    debug.assertThrow(finishEventSeen);
    debug.assertThrow(commandErrorBranchSeen);
    debug.assertThrow(event.type === "finish");
    debug.assertThrow(event.code === 0);
  },

  onCommand(id, data) {
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

  onExecution(id, event) {
    debug.assertThrow(id === 7);

    if (event.type === "output") {
      debug.assertThrow(event.stdout === "hello from stdout");
      debug.assertThrow(event.stderr === "hello from stderr");
      outputEventSeen = true;
      return;
    }

    debug.assertThrow(event.type === "finish");
    debug.assertThrow(event.code === 0);
    finishEventSeen = true;
  },
});
