/**
 * Entry point for the catter script runtime API.
 *
 * Import this module to access the namespace-style helpers exposed by catter,
 * including filesystem utilities, option parsing helpers, I/O streams, and
 * service lifecycle hooks.
 *
 * @example
 * ```typescript
 * import { fs, io, option, service } from "catter";
 *
 * io.println(fs.pwd());
 * ```
 */
import * as debug from "./debug.js";
import * as io from "./io.js";
import * as os from "./os.js";
import * as fs from "./fs.js";
import * as time from "./time.js";
import * as http from "./http.js";
import * as service from "./service.js";
import * as option from "./option/index.js";
import * as scripts from "./scripts/index.js";
import * as cmd from "./cmd/index.js";
import * as data from "./data/index.js";
import * as cli from "./cli/index.js";

export {
  debug,
  io,
  os,
  fs,
  time,
  http,
  option,
  service,
  scripts,
  cmd,
  data,
  cli,
};
export * from "./service.js";
export * from "./option/types.js";
export type * from "./cli/index.js";
