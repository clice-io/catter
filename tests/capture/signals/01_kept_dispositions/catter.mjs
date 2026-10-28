// catter itself started as nohup starts a program, with the options it is
// given: the build keeps what
// catter was given.
import { run } from "./nohup.mjs";

run(process.env.CATTER_EXECUTABLE, "tree", ...process.argv.slice(2), "-w", "1000", "--", "./report");
