import { stdout_print } from "catter-c";

// Regression coverage for the native "catter-c" module: a direct subpath-less
// import must resolve to the C module registered by the runtime, independent
// of the builtin ESM module table.
stdout_print("catter-c native module works\n");
