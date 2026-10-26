// Says it is ready, then runs until something ends it: says so when SIGTERM
// does, and ends by it.
process.on("SIGTERM", () => {
  process.stdout.write("the build got SIGTERM\n", () => {
    process.removeAllListeners("SIGTERM");
    process.kill(process.pid, "SIGTERM");
  });
});
console.log("ready");
setInterval(() => undefined, 1000);
