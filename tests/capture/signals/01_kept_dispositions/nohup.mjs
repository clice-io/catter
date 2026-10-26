// Python ignores SIGHUP, as nohup does, and SIGCHLD, blocks SIGUSR1, and runs
// the program its arguments name in its place, which keeps them, as exec
// keeps them. Python ignores SIGPIPE itself.
import { spawnSync } from "node:child_process";

const nohup =
  "import os, signal, sys; " +
  "signal.signal(signal.SIGHUP, signal.SIG_IGN); " +
  "signal.signal(signal.SIGCHLD, signal.SIG_IGN); " +
  "signal.pthread_sigmask(signal.SIG_BLOCK, [signal.SIGUSR1]); " +
  "os.execvp(sys.argv[1], sys.argv[1:])";

export function run(...command) {
  const { stdout } = spawnSync("python", ["-c", nohup, ...command], {
    stdio: ["ignore", "pipe", "inherit"],
  });
  process.stdout.write(stdout);
}
