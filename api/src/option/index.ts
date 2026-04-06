import { option_get_info, option_parse } from "catter-c";
import { OptionKindClass } from "./types.js";
import type { OptionInfo, OptionItem, OptionTable } from "./types.js";
import { io } from "../index.js";

/**
 * Helpers for working with generated compiler option tables.
 *
 * This module re-exports table-specific enums for supported toolchains and
 * provides utilities for collecting, normalizing, rendering, and rewriting
 * parsed `OptionItem` values.
 *
 * @example
 * ```typescript
 * import { option } from "catter";
 *
 * const parsed = option.collect("clang", ["-Iinclude", "main.cc"]);
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

function cloneOptionItem(item: OptionItem): OptionItem {
  return {
    ...item,
    values: [...item.values],
  };
}

function joinedTokens(key: string, values: string[]): string[] {
  if (values.length === 0) {
    return [key];
  }
  return [key + values[0], ...values.slice(1)];
}

function renderTokens(info: OptionInfo, item: OptionItem): string[] {
  if (info.flags & RENDER_JOINED) {
    return joinedTokens(item.key, item.values);
  }
  if (info.flags & RENDER_SEPARATE) {
    return [item.key, ...item.values];
  }

  switch (info.kind) {
    case OptionKindClass.GroupClass:
    case OptionKindClass.InputClass:
    case OptionKindClass.UnknownClass:
      return [item.key];
    case OptionKindClass.FlagClass:
    case OptionKindClass.ValuesClass:
    case OptionKindClass.SeparateClass:
    case OptionKindClass.JoinedOrSeparateClass:
    case OptionKindClass.RemainingArgsClass:
    case OptionKindClass.MultiArgClass:
      return [item.key, ...item.values];
    case OptionKindClass.JoinedClass:
    case OptionKindClass.RemainingArgsJoinedClass:
      return joinedTokens(item.key, item.values);
    case OptionKindClass.CommaJoinedClass:
      return item.values.length === 0
        ? [item.key]
        : [item.key + item.values.join(",")];
    case OptionKindClass.JoinedAndSeparateClass:
      return joinedTokens(item.key, item.values);
    default:
      return [item.key, ...item.values];
  }
}

/**
 * Rewrites an aliased parsed option so that it uses its canonical option ID and key.
 *
 * This mutates `item` in place and returns the same object for convenience.
 *
 * @param table - The option table that defines how aliases in `item` should be resolved, such as `"clang"` or `"nvcc"`.
 * @param item - The parsed option item to normalize. If `item.unalias` is `undefined`, the item is returned unchanged.
 * @returns The same `OptionItem` instance after canonicalization.
 *
 * @example
 * ```typescript
 * import { option } from "catter";
 *
 * const parsed = option.collect("nvcc", ["-ofoo.o"]);
 * if (Array.isArray(parsed)) {
 *   option.convertToUnalias("nvcc", parsed[0]);
 *   println(parsed[0].key); // "--output-file"
 * }
 * ```
 */
export function convertToUnalias(
  table: OptionTable,
  item: OptionItem,
): OptionItem {
  if (item.unalias === undefined) {
    return item;
  }

  const aliasInfo = option_get_info(table, item.id) as OptionInfo;
  const unaliasInfo = option_get_info(table, item.unalias) as OptionInfo;

  item.id = item.unalias;
  item.unalias = undefined;
  if (
    unaliasInfo.kind !== OptionKindClass.InputClass &&
    unaliasInfo.kind !== OptionKindClass.UnknownClass
  ) {
    item.key = unaliasInfo.prefixedKey;
  }
  item.values.push(...aliasInfo.aliasArgs);
  if (
    aliasInfo.kind === OptionKindClass.FlagClass &&
    aliasInfo.aliasArgs.length === 0 &&
    unaliasInfo.kind === OptionKindClass.JoinedClass
  ) {
    item.values.push("");
  }

  return item;
}

/**
 * Renders a parsed option item back into a command-line fragment.
 *
 * Aliased items are normalized on a cloned copy before rendering, so the input
 * item is not mutated by this helper.
 *
 * @param table - The option table that should be used to interpret the item.
 * @param item - The parsed option item to stringify.
 * @returns The rendered command-line fragment for `item`.
 *
 * @example
 * ```typescript
 * import { option } from "catter";
 *
 * const parsed = option.collect("nvcc", ["-I=include"]);
 * if (Array.isArray(parsed)) {
 *   println(option.stringify("nvcc", parsed[0]));
 * }
 * ```
 */
export function stringify(table: OptionTable, item: OptionItem): string {
  const renderItem =
    item.unalias === undefined
      ? item
      : convertToUnalias(table, cloneOptionItem(item));
  const info = option_get_info(table, renderItem.id) as OptionInfo;
  return renderTokens(info, renderItem).join(" ");
}

/**
 * Parses a full argument array and collects every parsed option item.
 *
 * @param table - The option table that should be used to interpret `args`.
 * @param args - The raw argument array, usually without the executable name.
 * @returns An array of parsed items when parsing succeeds, or the first parser error string.
 *
 * @example
 * ```typescript
 * import { option } from "catter";
 *
 * const parsed = option.collect("clang", ["-Iinclude", "main.cc"]);
 * if (!Array.isArray(parsed)) {
 *   throw new Error(parsed);
 * }
 * ```
 */
export function collect(
  table: OptionTable,
  args: string[],
  visibility = ALL_OPTION_VISIBILITY,
): OptionItem[] | string {
  let res: OptionItem[] | string = [];
  option_parse(
    table,
    args,
    (parseRes) => {
      if (typeof parseRes === "string") {
        res = parseRes;
        return false;
      }
      (res as OptionItem[]).push(parseRes);
      return true;
    },
    visibility,
  );
  return res;
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
 * import { option } from "catter";
 *
 * const rewritten = option.replace("clang", ["-Iold", "main.cc"], (parseRes) => {
 *   if (typeof parseRes === "string") {
 *     throw new Error(parseRes);
 *   }
 *   if (parseRes.key === "-I") {
 *     return { ...parseRes, values: ["include"] };
 *   }
 * });
 * ```
 */
export function replace(
  table: OptionTable,
  args: string[],
  cb: (
    parseRes: string | Readonly<OptionItem>,
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
    if (prevIndex != -1) {
      concatParts(typeof parseRes === "string" ? args.length : parseRes.index);
      prevIndex = -1;
    }
    const cbRes = cb(parseRes);
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

    if (typeof parseRes !== "string") {
      prevIndex = parseRes.index;
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
 * option.parse("clang", ["-Iinclude", "main.cc"], (parseRes) => {
 *   if (typeof parseRes === "string") {
 *     throw new Error(parseRes);
 *   }
 *   println(parseRes.key);
 *   return true;
 * });
 * ```
 */
export function parse(
  table: OptionTable,
  args: string[],
  cb: (parseRes: string | OptionItem) => boolean,
  visibility = ALL_OPTION_VISIBILITY,
): void {
  option_parse(table, args, cb, visibility);
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
 * const parsed = option.collect("nvcc", ["--help"]);
 * if (Array.isArray(parsed)) {
 *   const meta = option.info("nvcc", parsed[0]);
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
 * not listed in `excludeID` and are not `UnknownClass` in `to` are preserved.
 *
 * @param from - The option table used to collect and split the original
 * argument array into per-option spans.
 * @param to - The option table used to inspect second-stage parsed items with
 * `info()`, for example to reject `UnknownClass` results.
 * @param args - Raw command-line arguments to inspect, without the
 * executable name.
 * @param excludeID - Option IDs that should cause a second-stage parsed span
 * to be discarded. Defaults to `[0]`, which is INVALID in LLVM option table.
 * @returns A flattened array containing only spans that pass the second-stage
 * filter, or the parser error string returned while collecting `from`.
 */
export function table2table(
  from: OptionTable,
  to: OptionTable,
  args: string[],
  excludeID: number[] = [/*invliad default*/ 0],
): string | string[] {
  const fromRes = collect(from, args);
  if (typeof fromRes === "string") {
    return fromRes;
  }
  const optArgs = fromRes.map((val, idx) => {
    if (idx == fromRes.length - 1) {
      return args.slice(val.index);
    }
    return args.slice(val.index, fromRes[idx + 1].index);
  });
  return optArgs
    .filter((optArg) => {
      const toCheck = collect(to, optArg);
      return (
        Array.isArray(toCheck) &&
        toCheck.every(
          (val) =>
            !excludeID.includes(val.id) &&
            info(to, val).kind != OptionKindClass.UnknownClass,
        )
      );
    })
    .flat();
}
