/**
 * Entry point for the catter script runtime API.
 *
 * Import this module to access the helpers exposed by catter, including
 * filesystem utilities, option parsing helpers, I/O streams, and service
 * lifecycle hooks.
 *
 * @example
 * ```typescript
 * import { pwd } from "catter/fs";
 * import { println } from "catter/io";
 * import { parse } from "catter/option";
 * import { register } from "catter/service";
 *
 * println(pwd());
 * ```
 */
import * as cli from "catter/cli";

export * from "catter/debug";
export * from "catter/io";
export * from "catter/os";
export * from "catter/fs";
export * from "catter/time";
export * from "catter/http";
export * from "catter/service";
export * from "catter/option";
export * from "catter/cmd";
export * from "catter/cdb";
export * from "catter/data";
export * from "catter/neverthrow";

export { cli };
