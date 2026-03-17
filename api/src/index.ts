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
import * as option from "./option/index.js";
import * as service from "./service.js";

export { debug, io, os, fs, option, service };
export * from "./service.js";
export type {
  OptionInfo,
  OptionItem,
  OptionKindClass,
  OptionTable,
} from "catter-c";
