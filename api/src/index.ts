/**
 * Entry point for the catter script runtime API.
 *
 * Import this module to access the namespace-style helpers exposed by catter,
 * including filesystem utilities, option parsing helpers, I/O streams, and
 * service lifecycle hooks.
 *
 * @example
 * ```typescript
 * import * as fs from "catter/fs";
 * import * as io from "catter/io";
 * import * as option from "catter/option";
 * import * as service from "catter/service";
 *
 * io.println(fs.pwd());
 * ```
 */
import * as debug from "catter/debug";
import * as io from "catter/io";
import * as os from "catter/os";
import * as fs from "catter/fs";
import * as time from "catter/time";
import * as http from "catter/http";
import * as service from "catter/service";
import * as option from "catter/option";
import * as cmd from "catter/cmd";
import * as cdb from "catter/cdb";
import * as data from "catter/data";
import * as cli from "catter/cli";
import * as neverthrow from "catter/neverthrow";

export {
  debug,
  io,
  os,
  fs,
  time,
  http,
  option,
  service,
  cmd,
  cdb,
  data,
  cli,
  neverthrow,
};
