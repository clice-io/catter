// Starts one program, which catter captures when its hook is in the build.
import { spawnSync } from "node:child_process";

spawnSync("node", ["-e", "0"]);
