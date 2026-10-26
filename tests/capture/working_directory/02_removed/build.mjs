// A build step removes the directory it runs in, then runs a program there,
// which the system lets it: Python enters `gone`, removes it, and runs the
// program in its place.
import { mkdirSync } from "node:fs";
import { spawnSync } from "node:child_process";

mkdirSync("gone");
const script =
  "import os; os.chdir('gone'); os.rmdir('../gone'); " +
  "os.execvp('python', ['python', '-c', 'print(\"ran\")'])";
const run = spawnSync("python", ["-c", script], { encoding: "utf8" });
console.log(`${run.stdout.trim()}, exit ${run.status}`);
