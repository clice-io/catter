// Runs catter with its stdout a pipe nobody reads any more, as a pipe to
// `head -1` is once head has ended: catter can print nothing, and ends as it
// does on a failure of its own. The build waits for its input to end, which
// ends once the pipe is closed, so that catter prints only after.
import { spawn } from "node:child_process";

const catter = spawn(
  process.env.CATTER_EXECUTABLE,
  ["tree", "--", "node", "-e", "process.stdin.resume()"],
  { stdio: ["pipe", "pipe", "inherit"] },
);
catter.stdout.on("close", () => catter.stdin.end());
catter.stdout.destroy();
catter.on("exit", (code, signal) => {
  console.log(`catter ended by ${signal ?? `exit code ${code}`}`);
});
