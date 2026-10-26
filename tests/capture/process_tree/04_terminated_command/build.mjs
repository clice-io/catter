// The build stops a command it started, as make does when it is stopped
// itself: the program gets the signal, not only the process catter put in
// between, and the build sees the command end by it.
import { spawn } from "node:child_process";
import { createInterface } from "node:readline";

const child = spawn("node", ["child.mjs"], { stdio: ["ignore", "pipe", "inherit"] });
createInterface({ input: child.stdout }).on("line", (line) => {
  if (line === "ready") {
    child.kill("SIGTERM");
  } else {
    console.log(line);
  }
});
child.on("close", (code, signal) => {
  console.log(`the command ended by ${signal ?? `exit code ${code}`}`);
});
