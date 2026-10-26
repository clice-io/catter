// A build starts a program as it is, then as nohup would.
import { spawnSync } from "node:child_process";
import { run } from "./nohup.mjs";

process.stdout.write(spawnSync("./report", { stdio: ["ignore", "pipe", "inherit"] }).stdout);
run("./report");
