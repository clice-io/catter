// Serves one request, as a build server does for a build of later: it runs
// a program and answers with what it printed, then exits.
import { execFileSync } from "node:child_process";
import { writeFileSync } from "node:fs";
import { createServer } from "node:net";

const server = createServer((socket) => {
  const output = execFileSync(
    "node",
    ["-e", "console.log(`compiled ${process.argv[1]}`)", "with space"],
    { encoding: "utf8" },
  );
  socket.end(output, () => server.close());
});
server.listen(0, "127.0.0.1", () => {
  writeFileSync("port.txt", String(server.address().port));
  console.log("listening");
});
