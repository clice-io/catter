import { assertThrow, coloredPrint, path, pwd } from "catter";
import { path as fsSubPath, pwd as fsSubPwd } from "catter/fs";
import { coloredPrint as ioSubColoredPrint } from "catter/io";

// The aggregate entry must keep resolving to the same module instances as the
// individual subpath modules (the loader caches modules by canonical name).
assertThrow(path.joinAll === fsSubPath.joinAll);
assertThrow(coloredPrint === ioSubColoredPrint);

// Sanity-check that the shared modules are usable through both forms.
assertThrow(typeof pwd() === "string");
assertThrow(typeof coloredPrint === "function");
assertThrow(fsSubPwd() === pwd());
assertThrow(typeof path.joinAll === "function");
