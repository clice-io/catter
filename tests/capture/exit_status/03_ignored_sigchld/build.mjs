// A program that ignores SIGCHLD runs another in its place, which keeps it
// ignored: the build still gets the exit code of the program that runs.
import { spawnSync } from "node:child_process";

const script =
  "import os, signal; signal.signal(signal.SIGCHLD, signal.SIG_IGN); " +
  "os.execvp('python', ['python', '-c', 'raise SystemExit(3)'])";
console.log(`exit ${spawnSync("python", ["-c", script]).status}`);
