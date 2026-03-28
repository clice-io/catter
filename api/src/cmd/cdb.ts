import type { CDBItem } from "./cdb-manager.js";
import { CompilerAnalysis } from "./compiler-cmd.js";
import { analyze } from "./default-registry.js";
import type { Analysis } from "./model.js";

/**
 * Minimal command payload needed to emit compilation database entries.
 *
 * @example
 * ```ts
 * const command: cmd.CDBCommand = {
 *   cwd: "/tmp/build",
 *   argv: ["clang", "-c", "src/main.c"],
 * };
 * ```
 */
export type CDBCommand = {
  /** Working directory of the captured command. */
  cwd: string;
  /** Raw argv of the captured command. */
  argv: string[];
};

/**
 * One logical compilation database entry derived from an analysis.
 *
 * @example
 * ```ts
 * const entry: cmd.CDBEntry = {
 *   file: "src/main.c",
 *   output: "main.o",
 * };
 * ```
 */
export type CDBEntry = {
  /** Translation unit source file recorded by the entry. */
  file: string;
  /** Optional object output associated with `file`. */
  output?: string;
};

/**
 * Extracts `compile_commands.json`-style entries from an analysis.
 *
 * Only compiler analyses that represent source-to-object compilation produce
 * entries here.
 *
 * @example
 * ```ts
 * const analysis = cmd.CompilerAnalysis.analyze(["clang", "-c", "main.c"]);
 * const entries = cmd.cdbEntriesOf(analysis);
 * ```
 */
export function cdbEntriesOf(analysis: Analysis | undefined): CDBEntry[] {
  return CompilerAnalysis.from(analysis)?.cdbEntries() ?? [];
}

/**
 * Checks whether an analysis can contribute to a compilation database.
 *
 * @example
 * ```ts
 * const ok = cmd.isCDB(cmd.CompilerAnalysis.analyze(["clang", "-c", "main.c"]));
 * ```
 */
export function isCDB(analysis: Analysis | undefined): boolean {
  return CompilerAnalysis.from(analysis)?.isCDB() ?? false;
}

/**
 * Converts a command plus its analysis into concrete `CDBItem` records.
 *
 * If `analysis` is omitted, the command is analyzed through the default
 * registry first.
 *
 * @example
 * ```ts
 * const items = cmd.cdbItemsOf({
 *   cwd: "/tmp/build",
 *   argv: ["clang", "-c", "src/main.c"],
 * });
 * ```
 */
export function cdbItemsOf(
  command: CDBCommand,
  analysis: Analysis | undefined = analyze(command.argv),
): CDBItem[] {
  return cdbEntriesOf(analysis).map((entry) => {
    const item: CDBItem = {
      directory: command.cwd,
      file: entry.file,
      arguments: [...command.argv],
    };

    if (entry.output !== undefined) {
      item.output = entry.output;
    }

    return item;
  });
}
