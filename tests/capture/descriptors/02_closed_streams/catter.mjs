// catter itself started with its stdin and stdout closed: the build gets them
// closed too. compile-commands -q writes nothing on stdout.
import { spawnSync } from "node:child_process";

const script =
  "import os, sys; os.close(0); os.close(1); " +
  "os.execv(sys.argv[1], sys.argv[1:])";
const catter = process.env.CATTER_EXECUTABLE;
const run = spawnSync("python", ["-c", script, catter, "compile-commands", "-q", "--", "./report"]);
process.stdout.write(run.stderr);
console.log(`catter exit ${run.status}`);
