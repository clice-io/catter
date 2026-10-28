import { closeSync, writeSync } from "node:fs";

writeSync(1, "running\n");
closeSync(1);
process.stdin.resume();
process.stdin.on("end", () => process.exit(0));
