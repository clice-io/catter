// Runs the command after the signal, catter the one CATTER_EXECUTABLE names,
// and sends it the signal once the build is ready, as a CI job that is
// cancelled or an IDE that stops a task does. Its output ends once every
// process holding it has ended, the build too.
import { spawn } from "node:child_process";
import { createInterface } from "node:readline";

const [signal, name, ...args] = process.argv.slice(2);
const program = name === "catter" ? process.env.CATTER_EXECUTABLE : name;
const child = spawn(program, args, { stdio: ["ignore", "pipe", "inherit"] });
createInterface({ input: child.stdout }).on("line", (line) => {
  if (line === "ready") {
    child.kill(signal);
  } else {
    console.log(line);
  }
});
let ended = "";
child.on("exit", (code, by) => {
  ended = by ?? `exit code ${code}`;
});
child.on("close", () => {
  console.log(`${name} ended by ${ended}`);
});
