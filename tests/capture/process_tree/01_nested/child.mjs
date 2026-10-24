import { spawnSync } from "node:child_process";

const depth = Number(process.argv[2]);
if (depth === 0) {
  console.log("leaf");
} else {
  const child = spawnSync("node", ["child.mjs", String(depth - 1)], {
    encoding: "utf8",
  });
  process.stdout.write(`${depth} -> ${child.stdout}`);
  process.exitCode = child.status ?? 1;
}
