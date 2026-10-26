// Starts a program with its stdout closed, then with its stdin and stdout
// closed, as a shell does for `program >&-` and `program <&- >&-`: Python
// closes them and runs the program in its place.
import { spawnSync } from "node:child_process";

for (const fds of ["1", "0, 1"]) {
  const script = `import os; [os.close(fd) for fd in (${fds},)]; os.execv("./report", ["./report"])`;
  process.stdout.write(spawnSync("python", ["-c", script]).stderr);
}
