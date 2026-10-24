import type { AnalyzedData } from "../model.js";
import type { UnwrappedCompilerCommand } from "./types.js";

/**
 * Unwraps compiler wrapper commands before identification.
 *
 * This is currently a placeholder stage that returns the argv unchanged while
 * preserving the original argv for future wrapper support.
 */
export function unwrapCompilerCommand(command: AnalyzedData): UnwrappedCompilerCommand {
    return {
        exe: command.exe,
        argv: [...command.argv],
    };
}
