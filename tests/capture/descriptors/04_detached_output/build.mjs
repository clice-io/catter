// Starts a daemon as Gradle does: it reads the daemon's output until the
// daemon closes it, which says it runs, and then tells the daemon to stop.
import { spawn } from "node:child_process";

const daemon = spawn("python", ["daemon.py"], { stdio: ["pipe", "pipe", "inherit"] });
let output = "";
daemon.stdout.on("data", (data) => (output += data));
daemon.stdout.on("end", () => {
    console.log(`the daemon said: ${output.trim()}`);
    daemon.stdin.end("stop\n");
});
daemon.on("exit", (code) => console.log(`the daemon stopped: ${code}`));
