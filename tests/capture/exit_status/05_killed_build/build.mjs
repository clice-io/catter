// A signal ends the build: catter ends by the same signal.
process.kill(process.pid, "SIGTERM");
