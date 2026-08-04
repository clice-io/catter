import type { CompilerIdentity } from "../types.js";
import {
  buildClangGnuDriverModel,
  collectClangDriverOptions,
} from "./clang-driver.js";
import { ClangVisibility } from "catter/option";
import type { CompilerParseResult } from "../types.js";

/**
 * Parses a GCC command with the clang driver option table as a temporary model.
 *
 * TODO: Replace the temporary clang/MSVC-compatible fallback reuse when GCC has
 * its own option table and parser semantics.
 */
export function parseGccCommand(
  cmd: readonly string[],
  identity: CompilerIdentity,
): CompilerParseResult {
  const args = cmd.slice(1);
  return buildClangGnuDriverModel(
    collectClangDriverOptions(args, ClangVisibility.DefaultVis),
    identity.dialect,
  );
}
