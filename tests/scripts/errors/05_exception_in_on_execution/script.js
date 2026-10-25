import { onExecution } from "catter/service";

onExecution((ctx) => {
  if (ctx.result.code !== 0) {
    throw new Error(`command ${ctx.id} exited with ${ctx.result.code}`);
  }
});
