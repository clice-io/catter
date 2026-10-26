// The build kills a command it started, as on a timeout or a cancelled job:
// the program itself stops, not only the process catter put in between.
import { spawn } from "node:child_process";
import { once } from "node:events";
import { createServer } from "node:net";

// The program connects back, and its connection closes once it is gone.
const server = createServer();
server.listen(0, "127.0.0.1");
await once(server, "listening");
const child = spawn("node", ["child.mjs"], {
  stdio: "ignore",
  env: { ...process.env, CATTER_TEST_PORT: String(server.address().port) },
});
const [connection] = await once(server, "connection");
server.close();

connection.on("error", () => undefined);
connection.on("close", () => console.log("the program is gone"));
child.kill("SIGKILL");
