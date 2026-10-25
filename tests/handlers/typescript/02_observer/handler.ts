// Observes: records when each command's callback starts and ends, around a
// wait of a turn of the event loop, which the SDK lets end before the next
// message, and prints the record once the build is done.
import { serve } from "@catter/sdk";

const seen: string[] = [];
await serve({
  async onCommand(command) {
    seen.push(`start ${command.id}`);
    await new Promise((resolve) => setImmediate(resolve));
    seen.push(`end ${command.id}`);
  },
  onFinish(finish) {
    console.log(`finish: code ${finish.code}, after ${seen.join(", ")}`);
  },
});
console.log("served");
