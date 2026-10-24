// Starting a program that does not exist fails to start it: build tools and
// shells rely on that error ("command not found", trying the next
// candidate).
import { spawnSync } from "node:child_process";

const child = spawnSync("catter-test-no-such-program", { encoding: "utf8" });
console.log(child.error === undefined ? `ran, exit ${child.status}` : `failed to start: ${child.error.code}`);
