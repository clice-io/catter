// Observes: records each command after waiting a turn of the event loop,
// which the SDK lets finish before the next message, and prints them once
// the build is done.
import { serve } from "@catter/sdk";

const seen: string[] = [];
await serve({
  async onCommand(command) {
    await new Promise((resolve) => setImmediate(resolve));
    seen.push(JSON.stringify(command.args.slice(1)));
  },
  onFinish(finish) {
    console.log(`finish: code ${finish.code}, after ${seen.join(" ")}`);
  },
});
console.log("served");
