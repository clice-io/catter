import { stdout_print } from "catter/native";

// Regression coverage for the native "catter/native" module: a direct import
// must resolve to the C module registered by the runtime, independent of the
// builtin ESM module table.
stdout_print("catter/native module works\n");
