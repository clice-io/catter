// Says it got SIGTERM, then ends by it.
process.on("SIGTERM", () => {
  process.stdout.write("the program got SIGTERM\n", () => {
    process.removeAllListeners("SIGTERM");
    process.kill(process.pid, "SIGTERM");
  });
});
console.log("ready");
setInterval(() => undefined, 1000);
