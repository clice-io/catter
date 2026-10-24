import { onCommand } from "catter/service";

onCommand((ctx) => {
  if (ctx.capture.isErr()) {
    return;
  }
  const command = ctx.capture.value;
  if (command.argv.includes("act-on-me")) {
    ctx.modify({ ...command, argv: [...command.argv, "added-by-script"] });
  }
});
