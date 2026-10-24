// Starts a program with an almost empty environment, as `env -i` or a
// sandboxing build tool does: only what it needs to be found and to start.
import { spawnSync } from "node:child_process";

const names = process.platform === "win32" ? ["PATH", "SystemRoot"] : ["PATH"];
const env = Object.fromEntries(
  names.flatMap((name) =>
    process.env[name] === undefined ? [] : [[name, process.env[name]]],
  ),
);
const child = spawnSync("node", ["child.mjs"], { encoding: "utf8", env });
process.stdout.write(child.stdout);
process.stderr.write(child.stderr);
console.log(`exit ${child.status}`);
