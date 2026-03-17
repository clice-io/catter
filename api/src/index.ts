/**
 * This is the standard library for catter script.
 * It provides basic APIs for catching commands and system I/O.
 */
import * as debug from "./debug.js";
import * as io from "./io.js";
import * as os from "./os.js";
import * as fs from "./fs.js";
import * as option from "./option/index.js";
import * as service from "./service.js";

export { debug, io, os, fs, option, service };
export * from "./service.js";
export type { OptionItem, OptionTable } from "catter-c";
