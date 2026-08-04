import * as catter from "catter";
import { debug, fs, io } from "catter";
import * as fsSub from "catter/fs";
import * as ioSub from "catter/io";

// The aggregate entry must keep resolving to the same module instances as the
// individual subpath modules (the loader caches modules by canonical name).
debug.assertThrow(fs.path.joinAll === fsSub.path.joinAll);
debug.assertThrow(io.coloredPrint === ioSub.coloredPrint);

// Sanity-check that the shared modules are usable through both forms.
debug.assertThrow(typeof fs.pwd() === "string");
debug.assertThrow(typeof io.coloredPrint === "function");
debug.assertThrow(fsSub.pwd() === fs.pwd());
debug.assertThrow(typeof catter.fs.path.joinAll === "function");
