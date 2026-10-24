import type { CompilerDialect, CompilerIdentity } from "../types.js";
import { CompilerUnsupportedError } from "../errors.js";
import { parseClangCommand } from "./clang.js";
import { parseGccCommand } from "./gcc.js";
import { parseMsvcCommand } from "./msvc.js";
import { parseNvccCommand } from "./nvcc.js";
import type { CompilerParseResult } from "../types.js";

/** Dispatches a command to the builtin parser selected by compiler identity. */
export function parseCompilerCommand(
    cmd: readonly string[],
    identity: CompilerIdentity,
): CompilerParseResult {
    switch (identity.dialect satisfies CompilerDialect) {
        case "clang":
            return parseClangCommand(cmd, identity);
        case "gcc":
            return parseGccCommand(cmd, identity);
        case "msvc":
            return parseMsvcCommand(cmd, identity);
        case "nvcc":
            return parseNvccCommand(cmd, identity);
        default:
            throw new CompilerUnsupportedError(`unsupported compiler dialect: ${identity.dialect}`);
    }
}
