import { assertThrow } from "catter/debug";
import { register } from "catter/service";

const serviceArg = "--from-service";

let outputEventSeen = false;
let commandErrorBranchSeen = false;

register({
  async onStart(config) {
    await Promise.resolve();
    assertThrow(config.scriptPath === "script.ts");
    assertThrow(config.scriptArgs.length === 2);
    assertThrow(config.options.log);
    assertThrow(config.options.stdioMode === "inherit");

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
    assertThrow(outputEventSeen);
    assertThrow(commandErrorBranchSeen);
    assertThrow(event.code === 0);
    assertThrow(event.stdout === "");
    assertThrow(event.stderr === "");
  },

  async onCommand(id, data) {
    await Promise.resolve();
    assertThrow(id === 7);

    if (data.isErr()) {
      assertThrow(data.error.msg === "spawn failed");
      commandErrorBranchSeen = true;
      return { type: "skip" };
    }

    assertThrow(data.value.cwd === "/tmp");
    assertThrow(data.value.exe === "clang++");
    assertThrow(data.value.argv.length === 3);
    assertThrow(data.value.argv[2] === "-c");
    assertThrow(data.value.parent === 41);

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
    assertThrow(id === 7);
    assertThrow(event.code === 0);
    assertThrow(event.stdout === "hello from stdout");
    assertThrow(event.stderr === "hello from stderr");
    outputEventSeen = true;
  },
});
