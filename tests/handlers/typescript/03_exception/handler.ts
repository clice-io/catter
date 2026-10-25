import { serve } from "@catter/sdk";

await serve({
  decides: true,
  onCommand() {
    throw "thrown from onCommand";
  },
});
