// A program that dies abnormally is not reported to its build tool as
// having succeeded.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["-e", "process.abort()"]);
console.log(`status ${child.status}, signal ${child.signal}`);
