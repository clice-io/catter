// The build kills a command it started, as on a timeout or a cancelled job:
// the program itself stops, not only the process catter put in between.
import { spawn } from "node:child_process";

const child = spawn("node", ["child.mjs"], { stdio: ["ignore", "pipe", "inherit"] });
child.stdout.once("data", (chunk) => {
  const pid = Number(String(chunk).trim());
  child.on("exit", () => {
    let alive = true;
    try {
      process.kill(pid, 0);
    } catch {
      alive = false;
    }
    console.log(`the program is ${alive ? "still running" : "gone"}`);
    if (alive) {
      process.kill(pid, "SIGKILL");
    }
  });
  child.kill("SIGKILL");
});
