import type { CDBItem } from "./cdb-manager.js";

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
 * Converts a command plus resolved entries into concrete `CDBItem` records.
 *
 * @example
 * ```ts
 * const items = cmd.cdbItemsOf({
 *   cwd: "/tmp/build",
 *   argv: ["clang", "-c", "src/main.c"],
 * }, [{ file: "src/main.c", output: "main.o" }]);
 * ```
 */
export function cdbItemsOf(
  command: CDBCommand,
  entries: Iterable<CDBEntry>,
): CDBItem[] {
  return Array.from(entries, (entry) => {
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
