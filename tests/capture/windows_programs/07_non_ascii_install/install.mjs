// Installs the catter under test in bin/, under the copy's root, with the
// DLLs beside it: the hooks, and a runtime a debug build may need.
import { copyFileSync, mkdirSync, readdirSync } from "node:fs";
import * as path from "node:path";

const from = path.dirname(process.env.CATTER_EXECUTABLE);
mkdirSync("bin");
for (const name of readdirSync(from)) {
  if (/^(catter|catter-proxy)\.exe$|\.dll$/.test(name)) {
    copyFileSync(path.join(from, name), path.join("bin", name));
  }
}
