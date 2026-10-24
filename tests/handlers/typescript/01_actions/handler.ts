// Drops one command, modifies another, and prints what it is told, on
// stderr: serve() points console.log there.
import { serve } from "@catter/sdk";

await serve({
  decides: true,
  onStart(session) {
    console.log(`start: mode ${session.mode}, build ${JSON.stringify(session.build.command)}`);
  },
  onCommand(command) {
    console.log(`command ${command.id}: ${JSON.stringify(command.args.slice(1))}`);
    if (command.args.includes("drop-me")) {
      return { action: "drop" };
    }
    if (command.args.includes("modify-me")) {
      return { action: "modify", args: [...command.args, "added-by-handler"] };
    }
    return undefined;
  },
  onExit(exit) {
    console.log(`exit ${exit.id}: code ${exit.code}`);
  },
  onFinish(finish) {
    console.log(`finish: code ${finish.code}`);
  },
});
