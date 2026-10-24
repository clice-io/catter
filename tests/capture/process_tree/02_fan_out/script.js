// Counts the programs build.mjs starts, which start at once: the order
// catter sees them in is not pinned, their number is.
import { println } from "catter/io";
import { onCommand, onFinish } from "catter/service";

const seen = new Set();
onCommand((ctx) => {
  if (ctx.capture.isOk() && ctx.capture.value.argv.includes("-e")) {
    seen.add(ctx.capture.value.argv.at(-1));
  }
});
onFinish(() => {
  println(`catter saw ${seen.size} programs`);
});
