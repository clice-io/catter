import { onCommand } from "catter/service";

onCommand((ctx) => {
  if (ctx.capture.isOk() && ctx.capture.value.argv.includes("drop-me")) {
    ctx.drop();
  }
});
