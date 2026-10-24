import type { CompilerIdentity } from "../types.js";
import {
    buildClangClDriverModel,
    CLANG_CL_VISIBILITY,
    collectClangDriverOptions,
} from "./clang-driver.js";
import type { CompilerParseResult } from "../types.js";

/**
 * Parses an MSVC-family command using the clang-cl compatible option table.
 *
 * MSVC-specific behavior lives here; clang-driver only provides the shared LLVM
 * option table adapter and clang driver semantics.
 */
export function parseMsvcCommand(
    cmd: readonly string[],
    identity: CompilerIdentity,
): CompilerParseResult {
    const args = cmd.slice(1);
    return buildClangClDriverModel(
        collectClangDriverOptions(args, CLANG_CL_VISIBILITY),
        identity.dialect,
    );
}
