import { option_get_info, option_parse } from "catter-c";
import { err, ok, type Result } from "catter/neverthrow";
import { OptionKindClass } from "./types.js";
import type { OptionInfo, OptionItem, OptionTable } from "./types.js";

/**
 * Helpers for working with generated compiler option tables.
 *
 * This module re-exports table-specific enums for supported toolchains and
 * provides utilities for collecting, normalizing, rendering, and rewriting
 * parsed `OptionItem` values.
 *
 * @example
 * ```typescript
 * import { collect } from "catter/option";
 *
 * const parsed = collect("clang", ["-Iinclude", "main.cc"]);
 * ```
 */

export { ClangID } from "./clang.js";
export { ClangFlag } from "./clang.js";
export { ClangVisibility } from "./clang.js";
export { ClangDriverClass } from "./clang.js";
export { LldCoffID } from "./lld-coff.js";
export { LldCoffFlag } from "./lld-coff.js";
export { LldCoffVisibility } from "./lld-coff.js";
export { LldElfID } from "./lld-elf.js";
export { LldElfFlag } from "./lld-elf.js";
export { LldElfVisibility } from "./lld-elf.js";
export { LldMachOID } from "./lld-macho.js";
export { LldMachOFlag } from "./lld-macho.js";
export { LldMachOVisibility } from "./lld-macho.js";
export { LldMinGWID } from "./lld-mingw.js";
export { LldMinGWFlag } from "./lld-mingw.js";
export { LldMinGWVisibility } from "./lld-mingw.js";
export { LldWasmID } from "./lld-wasm.js";
export { LldWasmFlag } from "./lld-wasm.js";
export { LldWasmVisibility } from "./lld-wasm.js";
export { LlvmDlltoolID } from "./llvm-dlltool.js";
export { LlvmDlltoolFlag } from "./llvm-dlltool.js";
export { LlvmDlltoolVisibility } from "./llvm-dlltool.js";
export { LlvmLibID } from "./llvm-lib.js";
export { LlvmLibFlag } from "./llvm-lib.js";
export { LlvmLibVisibility } from "./llvm-lib.js";
export { NvccID } from "./nvcc.js";
export { NvccFlag } from "./nvcc.js";
export { NvccVisibility } from "./nvcc.js";
export { OptionKindClass } from "./types.js";
export type { OptionInfo, OptionItem, OptionTable } from "./types.js";

const RENDER_JOINED = 1 << 2;
const RENDER_SEPARATE = 1 << 3;
const ALL_OPTION_VISIBILITY = 0xffff_ffff;

function joinedTokens(key: string, values: string[]): string[] {
  if (values.length === 0) {
    return [key];
  }
  return [key + values[0], ...values.slice(1)];
}

function renderTokensCanonical(info: OptionInfo, item: OptionItem): string[] {
  if (info.flags & RENDER_JOINED) {
    return joinedTokens(info.prefixedKey, item.values);
  }
  if (info.flags & RENDER_SEPARATE) {
    return [info.prefixedKey, ...item.values];
  }

  switch (info.kind) {
    case OptionKindClass.Group:
    case OptionKindClass.Input:
    case OptionKindClass.Unknown:
      return [item.key, ...item.values];
    case OptionKindClass.Joined:
    case OptionKindClass.JoinedAndSeparate:
      return joinedTokens(info.prefixedKey, item.values);
    case OptionKindClass.CommaJoined:
      return item.values.length === 0
        ? [info.prefixedKey]
        : [info.prefixedKey + item.values.join(",")];
    case OptionKindClass.Flag:
    case OptionKindClass.Values:
    case OptionKindClass.Separate:
    case OptionKindClass.MultiArg:
    case OptionKindClass.JoinedOrSeparate:
    case OptionKindClass.RemainingArgs:
    case OptionKindClass.RemainingArgsJoined:
      return [info.prefixedKey, ...item.values];
    default:
      return [item.key, ...item.values];
  }
}

/**
 * Renders a parsed option item back into a command-line fragment.
 *
 * Parsed items are already unaliased: `item.id` is the canonical option ID and
 * `item.values` already contains any alias-provided arguments, so the item is
 * rendered using the canonical option spelling from the table.
 *
 * @param table - The option table that should be used to interpret the item.
 * @param item - The parsed option item to stringify.
 * @returns The rendered command-line fragment for `item`.
 *
 * @example
 * ```typescript
 * import { collect, stringify } from "catter/option";
 *
 * const parsed = collect("nvcc", ["-I=include"]);
 * if (parsed.isOk()) {
 *   println(stringify("nvcc", parsed.value[0]));
 * }
 * ```
 */
export function stringify(table: OptionTable, item: OptionItem): string {
  const info = option_get_info(table, item.id) as OptionInfo;
  return renderTokensCanonical(info, item).join(" ");
}

/**
 * Parses a full argument array and collects every parsed option item.
 *
 * @param table - The option table that should be used to interpret `args`.
 * @param args - The raw argument array, usually without the executable name.
 * @returns A `Result` whose `Ok` value is the parsed items, or whose `Err`
 * value is the first parser error string.
 *
 * @example
 * ```typescript
 * import { collect } from "catter/option";
 *
 * const parsed = collect("clang", ["-Iinclude", "main.cc"]);
 * if (parsed.isErr()) {
 *   throw new Error(parsed.error);
 * }
 * ```
 */
export function collect(
  table: OptionTable,
  args: string[],
  visibility = ALL_OPTION_VISIBILITY,
): Result<OptionItem[], string> {
  const items: OptionItem[] = [];
  let failure: string | undefined;
  option_parse(
    table,
    args,
    (parseRes) => {
      if (typeof parseRes === "string") {
        failure = parseRes;
        return false;
      }
      items.push(parseRes);
      return true;
    },
    visibility,
  );
  return failure === undefined ? ok(items) : err(failure);
}

/**
 * Parses `args` and rewrites matched options while preserving untouched spans.
 *
 * @param table - The option table that should be used to interpret `args`.
 * @param args - The raw argument array to inspect and rewrite.
 * @param cb - Callback invoked for each parser result. Return `undefined` to keep the original text, an `OptionItem` or string value to replace the current parsed segment, or a boolean to continue or stop parsing without rewriting the current segment.
 * @returns The rewritten arguments joined into a single space-delimited command string.
 *
 * @example
 * ```typescript
 * import { replace } from "catter/option";
 *
 * const rewritten = replace("clang", ["-Iold", "main.cc"], (parseRes) => {
 *   if (parseRes.isErr()) {
 *     throw new Error(parseRes.error);
 *   }
 *   if (parseRes.value.key === "-I") {
 *     return { ...parseRes.value, values: ["include"] };
 *   }
 * });
 * ```
 */
export function replace(
  table: OptionTable,
  args: string[],
  cb: (
    parseRes: Result<Readonly<OptionItem>, string>,
  ) => OptionItem | boolean | undefined | string | string[],
): string {
  let nextToAdd = 0;
  let prevIndex = -1;
  let newPart = "";
  let finalArgs: string[] = [];
  const concatParts = (endIndex: number) => {
    finalArgs = finalArgs.concat([
      ...args.slice(nextToAdd, prevIndex),
      newPart,
    ]);
    nextToAdd = endIndex;
  };
  option_parse(table, args, (parseRes) => {
    const res: Result<Readonly<OptionItem>, string> = typeof parseRes ===
    "string"
      ? err(parseRes)
      : ok(parseRes);
    if (prevIndex != -1) {
      concatParts(res.isErr() ? args.length : res.value.index);
      prevIndex = -1;
    }
    const cbRes = cb(res);
    if (cbRes === undefined) {
      return true;
    }
    if (typeof cbRes === "boolean") {
      return cbRes;
    }

    if (typeof cbRes === "string") {
      newPart = cbRes;
    } else if (Array.isArray(cbRes)) {
      newPart = cbRes.join(" ");
    } else {
      newPart = stringify(table, cbRes);
    }

    if (res.isOk()) {
      prevIndex = res.value.index;
    }
    return true;
  });
  if (prevIndex != -1) {
    concatParts(args.length);
  }
  prevIndex = args.length;
  concatParts(-1);
  finalArgs.pop();
  return finalArgs.join(" ");
}

/**
 * Streams parser results to a callback without collecting them first.
 *
 * @param table - The option table that should be used to interpret `args`, such as `"clang"` or `"nvcc"`.
 * @param args - The raw argument array to parse, usually without the executable name.
 * @param cb - Callback invoked for each parser result. It receives either a parsed `OptionItem` or an error string, and should return `true` to continue parsing or `false` to stop early.
 * @returns Nothing. Parsing side effects are delivered through `cb`.
 *
 * @example
 * ```typescript
 * import { parse } from "catter/option";
 *
 * parse("clang", ["-Iinclude", "main.cc"], (parseRes) => {
 *   if (parseRes.isErr()) {
 *     throw new Error(parseRes.error);
 *   }
 *   println(parseRes.value.key);
 *   return true;
 * });
 * ```
 */
export function parse(
  table: OptionTable,
  args: string[],
  cb: (parseRes: Result<OptionItem, string>) => boolean,
  visibility = ALL_OPTION_VISIBILITY,
): void {
  option_parse(
    table,
    args,
    (parseRes) => {
      return cb(typeof parseRes === "string" ? err(parseRes) : ok(parseRes));
    },
    visibility,
  );
}

/**
 * Returns metadata for a parsed option item.
 *
 * @param table - The option table that was used to produce `item`.
 * @param item - The parsed option item whose metadata should be looked up.
 * @returns The `OptionInfo` record associated with `item.id` in `table`.
 *
 * @example
 * ```typescript
 * import { collect, info } from "catter/option";
 *
 * const parsed = collect("nvcc", ["--help"]);
 * if (parsed.isOk()) {
 *   const meta = info("nvcc", parsed.value[0]);
 *   println(meta.prefixedKey);
 * }
 * ```
 */
export function info(table: OptionTable, item: OptionItem) {
  return option_get_info(table, item.id) as OptionInfo;
}

/**
 * Re-parses argument spans collected from one option table and keeps only the
 * spans that pass a second-stage filter.
 *
 * The input is first collected with `from`, then split back into the original
 * argument slices that produced each parsed item. Each slice is parsed again
 * with the current second-stage parser, and only slices whose parsed items are
 * not listed in `excludeID` and are not `Unknown` in `to` are preserved.
 *
 * @param from - The option table used to collect and split the original
 * argument array into per-option spans.
 * @param to - The option table used to inspect second-stage parsed items with
 * `info()`, for example to reject `Unknown` results.
 * @param args - Raw command-line arguments to inspect, without the
 * executable name.
 * @param excludeID - Option IDs that should cause a second-stage parsed span
 * to be discarded. Defaults to `[0]`, which is INVALID in LLVM option table.
 * @returns A `Result` whose `Ok` value is the flattened array containing only
 * spans that pass the second-stage filter, or whose `Err` value is the parser
 * error string returned while collecting `from`.
 */
export function table2table(
  from: OptionTable,
  to: OptionTable,
  args: string[],
  excludeID: number[] = [/*invliad default*/ 0],
): Result<string[], string> {
  const fromRes = collect(from, args);
  if (fromRes.isErr()) {
    return err(fromRes.error);
  }
  const optArgs = fromRes.value.map((val, idx) => {
    if (idx == fromRes.value.length - 1) {
      return args.slice(val.index);
    }
    return args.slice(val.index, fromRes.value[idx + 1].index);
  });
  return ok(
    optArgs
      .filter((optArg) => {
        const toCheck = collect(to, optArg);
        return (
          toCheck.isOk() &&
          toCheck.value.every(
            (val) =>
              !excludeID.includes(val.id) &&
              info(to, val).kind != OptionKindClass.Unknown,
          )
        );
      })
      .flat(),
  );
}
