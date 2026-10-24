// A program writes every byte value, NUL and line ends included, to stdout
// and stderr; its parent receives them byte for byte.
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";

const child = spawnSync("node", ["child.mjs"], { maxBuffer: 1 << 30 });
const digest = (data) => createHash("sha256").update(data).digest("hex");
console.log(`stdout: ${child.stdout.length} bytes, sha256 ${digest(child.stdout)}`);
console.log(`stderr: ${child.stderr.length} bytes, sha256 ${digest(child.stderr)}`);
process.exitCode = child.status ?? 1;
