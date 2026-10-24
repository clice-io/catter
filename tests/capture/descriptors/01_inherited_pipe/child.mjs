// Where the pipe is gone (catter#153), whatever took descriptor 3 gets the
// write or refuses it, which differs from build to build: only what reaches
// the parent is the case's.
import * as fs from "node:fs";

try {
    fs.writeSync(3, "through fd");
} catch {}
