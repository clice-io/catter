// Says it is ready, then runs until something ends it: says so when SIGINT
// does, and ends by it.
process.on("SIGINT", () => {
  process.stdout.write("the build got SIGINT\n", () => {
    process.removeAllListeners("SIGINT");
    process.kill(process.pid, "SIGINT");
  });
});
console.log("ready");
setInterval(() => undefined, 1000);
