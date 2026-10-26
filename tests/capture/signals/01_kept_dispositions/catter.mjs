// catter itself started as nohup starts a program: the build keeps what
// catter was given.
import { run } from "./nohup.mjs";

run(process.env.CATTER_EXECUTABLE, "tree", "-w", "1000", "--", "./report");
