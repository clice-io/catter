// Starts a program with an environment of its own: variables added, empty,
// non-ASCII and removed.
import { spawnSync } from "node:child_process";

const env = {
  ...process.env,
  CATTER_PROBE_SPACES: "a b  c",
  CATTER_PROBE_UNICODE: "中文-é",
  CATTER_PROBE_EMPTY: "",
};
delete env.CATTER_PROBE_REMOVED;
const child = spawnSync(
  "node",
  [
    "child.mjs",
    "CATTER_PROBE_SPACES",
    "CATTER_PROBE_UNICODE",
    "CATTER_PROBE_EMPTY",
    "CATTER_PROBE_REMOVED",
  ],
  { encoding: "utf8", env },
);
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
