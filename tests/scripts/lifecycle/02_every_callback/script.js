// Prints what each callback receives. Its arguments steer onStart: `append`
// adds an argument to the build command, `stop` runs no build.
import { path } from "catter/fs";
import { println } from "catter/io";
import { register } from "catter/service";

register({
  onStart(config) {
    println(
      `onStart: args ${JSON.stringify(config.scriptArgs)}, build ${JSON.stringify(config.buildSystemCommand)}, ` +
        `cwd ${config.buildSystemCommandCwd}, stdio ${config.options.stdioMode}, runtime ${JSON.stringify(config.runtime)}`,
    );
    if (config.scriptArgs.includes("stop")) {
      return { ...config, execute: false };
    }
    if (config.scriptArgs.includes("append")) {
      return { ...config, buildSystemCommand: [...config.buildSystemCommand, "added-by-onStart"] };
    }
  },

  onCommand(id, capture) {
    if (capture.isErr()) {
      println(`onCommand ${id}: error ${JSON.stringify(capture.error.msg)}, parent ${capture.error.parent}`);
      return { type: "skip" };
    }
    const command = capture.value;
    println(
      `onCommand ${id}: parent ${command.parent}, args ${JSON.stringify(command.argv.slice(1))}, ` +
        `cwd ${command.cwd}, runtime ${JSON.stringify(command.runtime)}`,
    );
    if (!command.argv.includes("child.mjs")) {
      return { type: "skip" };
    }
    return {
      type: "modify",
      data: {
        ...command,
        cwd: path.joinAll(command.cwd, "sub"),
        argv: [command.argv[0], path.joinAll(command.cwd, "child.mjs"), "added-by-onCommand"],
        env: [...command.env, "CATTER_CASE_VAR=set-by-onCommand"],
      },
    };
  },

  onExecution(id, result) {
    println(
      `onExecution ${id}: code ${result.code}, stdout ${JSON.stringify(result.stdout)}, stderr ${JSON.stringify(result.stderr)}`,
    );
  },

  onFinish(result) {
    println(
      `onFinish: code ${result.code}, stdout ${JSON.stringify(result.stdout)}, stderr ${JSON.stringify(result.stderr)}`,
    );
  },
});
