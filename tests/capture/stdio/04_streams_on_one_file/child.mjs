// Says what its stdout and stderr are, then writes to them in turn.
import { fstatSync, writeSync } from "node:fs";

const kind = (fd) => {
  const stat = fstatSync(fd);
  return stat.isFile() ? "a file" : stat.isFIFO() ? "a pipe" : "neither file nor pipe";
};
writeSync(1, `child: stdout is ${kind(1)}, stderr is ${kind(2)}\n`);
for (let i = 1; i <= 3; i += 1) {
  writeSync(1, `child: stdout ${i}\n`);
  writeSync(2, `child: stderr ${i}\n`);
}
