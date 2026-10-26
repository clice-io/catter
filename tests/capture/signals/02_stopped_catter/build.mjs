// Says it is ready, then runs until its input ends; says how it ends.
process.on("SIGTERM", () => {
  process.stdout.write("the build got SIGTERM\n", () => {
    process.removeAllListeners("SIGTERM");
    process.kill(process.pid, "SIGTERM");
  });
});
process.stdin.on("end", () => console.log("the build ran to the end of its input"));
process.stdin.resume();
console.log("ready");
