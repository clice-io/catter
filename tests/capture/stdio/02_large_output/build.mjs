// A program writes 4 MiB to stdout and to stderr at once; its parent reads
// both to the end.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["child.mjs"], { maxBuffer: 1 << 30 });
console.log(`stdout: ${child.stdout.length} bytes, all o: ${child.stdout.every((byte) => byte === 0x6f)}`);
console.log(`stderr: ${child.stderr.length} bytes, all e: ${child.stderr.every((byte) => byte === 0x65)}`);
process.exitCode = child.status ?? 1;
