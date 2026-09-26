import { onCommand } from "catter/service";

onCommand(() => {
  throw new Error("thrown from onCommand");
});
