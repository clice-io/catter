// A build step runs a program with a HOME of its own, as test harnesses and
// sandboxes set one, and without CATTER_HOME: catter keeps its files in its
// own data directory, and leaves the new HOME as the step made it.
import { mkdirSync } from "node:fs";
import { spawnSync } from "node:child_process";
import * as path from "node:path";

mkdirSync("home");
const env = { ...process.env, HOME: path.resolve("home") };
delete env.CATTER_HOME;
const run = spawnSync("node", ["-e", "console.log('ran')"], { env, encoding: "utf8" });
console.log(`${run.stdout.trim()}, exit ${run.status}`);
