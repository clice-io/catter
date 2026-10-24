// Starts server.mjs as a build tool starts its server (an sccache server, a
// Gradle daemon): detached, outliving the build. It is ready once it wrote
// its port.
import { spawn } from "node:child_process";

const server = spawn("node", ["server.mjs"], {
  detached: true,
  stdio: ["ignore", "pipe", "ignore"],
});
server.stdout.once("data", () => {
  server.stdout.destroy();
  server.unref();
  console.log("server started");
});
