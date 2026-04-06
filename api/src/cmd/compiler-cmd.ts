import type { Compiler } from "catter-c";
import { identify_compiler } from "catter-c";

import * as fs from "../fs.js";
import * as os from "../os.js";
import * as option from "../option/index.js";
import { ClangID, ClangVisibility } from "../option/clang.js";
import {
  OptionKindClass,
  type OptionInfo,
  type OptionItem,
  type OptionTable,
} from "../option/types.js";
import { Analysis } from "./model.js";
import type { Analysis as AnyAnalysis, Analyzer, Edge } from "./model.js";

/**
 * Legacy compiler command categories preserved for compatibility.
 *
 * Prefer the newer `phase` + `artifact` model on `CompilerAnalysis` for
 * new code.
 *
 * @example
 * ```ts
 * const kind = cmd.CompilerType.SourceToObject;
 * ```
 */
export const CompilerType = {
  SourceToObject: 0,
  ObjectToExe: 1,
  ObjectToShare: 2,
  ObjectToLib: 3,
  SourceToExe: 4,
  SourceToAsm: 5,
  SourceToLlvmIR: 6,
  SourceToLlvmBC: 7,
  SourceToPch: 8,
  SourceToPcm: 9,
  SourcePreprocess: 10,
  SourceSyntaxOnly: 11,
  RelocatableLink: 12,
} as const;

/**
 * Union type of `CompilerType` values.
 *
 * @example
 * ```ts
 * const kind: cmd.CompilerType = cmd.CompilerType.SourceToExe;
 * ```
 */
export type CompilerType = (typeof CompilerType)[keyof typeof CompilerType];

/**
 * High-level compiler pipeline phase.
 *
 * @example
 * ```ts
 * const phase = cmd.CompilerPhase.Compile;
 * ```
 */
export const CompilerPhase = {
  Preprocess: "preprocess",
  SyntaxOnly: "syntax-only",
  Compile: "compile",
  Link: "link",
  Archive: "archive",
  RelocatableLink: "relocatable-link",
  DeviceLink: "device-link",
} as const;

/**
 * Union type of `CompilerPhase` values.
 *
 * @example
 * ```ts
 * const phase: cmd.CompilerPhase = cmd.CompilerPhase.Link;
 * ```
 */
export type CompilerPhase = (typeof CompilerPhase)[keyof typeof CompilerPhase];

/**
 * Main artifact kind produced by a compiler command.
 *
 * @example
 * ```ts
 * const artifact = cmd.CompilerArtifact.Object;
 * ```
 */
export const CompilerArtifact = {
  None: "none",
  Stdout: "stdout",
  Object: "object",
  Executable: "exe",
  SharedLibrary: "shared",
  StaticLibrary: "static-lib",
  Assembly: "asm",
  LlvmIR: "llvm-ir",
  LlvmBitcode: "llvm-bc",
  Pch: "pch",
  Pcm: "pcm",
  Ptx: "ptx",
  Cubin: "cubin",
  Fatbin: "fatbin",
  Unknown: "unknown",
} as const;

/**
 * Union type of `CompilerArtifact` values.
 *
 * @example
 * ```ts
 * const artifact: cmd.CompilerArtifact = cmd.CompilerArtifact.SharedLibrary;
 * ```
 */
export type CompilerArtifact =
  (typeof CompilerArtifact)[keyof typeof CompilerArtifact];

/**
 * Supported compiler executable identifiers.
 *
 * @example
 * ```ts
 * const exe: cmd.CompilerExe = "clang";
 * ```
 */
export type CompilerExe = Extract<Compiler, "clang" | "gcc">;

/**
 * Driver option syntax style observed during parsing.
 *
 * @example
 * ```ts
 * const style: cmd.CompilerStyle = "gnu";
 * ```
 */
export type CompilerStyle = "gnu" | "cl";

/**
 * One parsed compiler input, including its inferred role.
 *
 * @example
 * ```ts
 * const input: cmd.CompilerInput = {
 *   path: "main.c",
 *   kind: "source",
 *   index: 1,
 * };
 * ```
 */
export interface CompilerInput {
  path: string;
  kind: "source" | "link";
  index: number;
}

type CompilerFamily = "clang-like" | "msvc-like" | "nvcc-like";
type OutputChannel = "primary" | "object" | "executable" | "linker";

type ParsedOption = {
  raw: OptionItem;
  rawInfo: OptionInfo;
  item: OptionItem;
  info: OptionInfo;
};

type OutputOption = {
  value: string;
  index: number;
  channel: OutputChannel;
};

type OutputArg = {
  value: string;
  optionIndex: number;
  valueIndex: number;
};

type CommandModel = {
  compiler: CompilerExe;
  style: CompilerStyle;
  phase: CompilerPhase;
  artifact: CompilerArtifact;
  explicitLanguage?: string;
  inputs: CompilerInput[];
  outputs: Partial<Record<OutputChannel, OutputOption>>;
};

type CompilerFamilyAnalyzer = (
  cmd: readonly string[],
  compiler: CompilerExe,
) => CommandModel;

const CompilerPhaseValue = CompilerPhase;
const CompilerArtifactValue = CompilerArtifact;
const LegacyTypeValue = CompilerType;

const SOURCE_SUFFIXES = new Set([
  ".c",
  ".cc",
  ".cp",
  ".cpp",
  ".cxx",
  ".c++",
  ".cu",
  ".hip",
  ".m",
  ".mm",
  ".s",
  ".sx",
  ".asm",
  ".f",
  ".f77",
  ".f90",
  ".f95",
  ".f03",
  ".f08",
  ".for",
  ".ftn",
  ".i",
  ".ii",
  ".mi",
  ".mii",
  ".bc",
  ".ll",
  ".pcm",
  ".pch",
  ".gch",
]);

const LINK_INPUT_SUFFIXES = new Set([
  ".o",
  ".obj",
  ".a",
  ".lib",
  ".lo",
  ".so",
  ".dylib",
  ".dll",
  ".exp",
  ".res",
]);

const STYLE_DEFAULT_EXTENSIONS: Record<
  CompilerStyle,
  {
    object: string;
    executable: string;
    shared: string;
    staticLibrary: string;
  }
> = {
  gnu: {
    object: ".o",
    executable: "",
    shared: "",
    staticLibrary: ".a",
  },
  cl: {
    object: ".obj",
    executable: ".exe",
    shared: ".dll",
    staticLibrary: ".lib",
  },
};

function cloneOptionItem(item: OptionItem): OptionItem {
  return {
    ...item,
    values: [...item.values],
  };
}

function normalizeOptionItem(table: OptionTable, item: OptionItem): OptionItem {
  return option.convertToUnalias(table, cloneOptionItem(item));
}

function collectParsedOptions(
  table: OptionTable,
  args: readonly string[],
  visibility?: number,
): ParsedOption[] {
  const collected = option.collect(table, [...args], visibility);
  if (!Array.isArray(collected)) {
    throw new Error(`fatal error in parsing: ${collected}`);
  }

  return collected.map((raw) => {
    const item = normalizeOptionItem(table, raw);
    return {
      raw,
      rawInfo: option.info(table, raw),
      item,
      info: option.info(table, item),
    };
  });
}

function clangLikeDriverVisibility(): number {
  if (os.platform() === "windows") {
    return ClangVisibility.DefaultVis | ClangVisibility.CLOption;
  }

  return ClangVisibility.DefaultVis;
}

function supportsClStyleAnalysis(): boolean {
  return os.platform() === "windows";
}

function endsWithPathSep(value: string): boolean {
  return value.endsWith("/") || value.endsWith("\\");
}

function pathStem(value: string): string {
  if (value === "-") {
    return "stdin";
  }

  const filename = fs.path.filename(value);
  const ext = fs.path.extension(filename);
  if (ext.length === 0 || ext.length >= filename.length) {
    return filename;
  }
  return filename.slice(0, filename.length - ext.length);
}

function classifyPathBySuffix(value: string): "source" | "link" {
  if (value === "-") {
    return "source";
  }

  const ext = fs.path.extension(value);
  if (ext === ".C" || ext === ".M") {
    return "source";
  }

  const lowerExt = ext.toLowerCase();
  if (SOURCE_SUFFIXES.has(lowerExt)) {
    return "source";
  }
  if (LINK_INPUT_SUFFIXES.has(lowerExt)) {
    return "link";
  }
  return "link";
}

function languageIsSource(language: string | undefined): boolean | undefined {
  if (language === undefined || language.length === 0) {
    return undefined;
  }

  const lower = language.toLowerCase();
  if (lower === "none") {
    return undefined;
  }
  if (lower === "object") {
    return false;
  }
  return true;
}

function classifyInputKind(
  value: string,
  explicitLanguage: string | undefined,
): "source" | "link" {
  const explicit = languageIsSource(explicitLanguage);
  if (explicit !== undefined) {
    return explicit ? "source" : "link";
  }
  return classifyPathBySuffix(value);
}

function recordInput(
  model: CommandModel,
  path: string | undefined,
  kind: "source" | "link",
  index: number,
): void {
  if (path === undefined) {
    return;
  }

  model.inputs.push({
    path,
    kind,
    index,
  });
}

function recordOutput(
  model: CommandModel,
  channel: OutputChannel,
  value: string | undefined,
  index: number,
): void {
  if (value === undefined) {
    return;
  }

  model.outputs[channel] = {
    value,
    index,
    channel,
  };
}

function pickLaterOutput(
  left: OutputOption | undefined,
  right: OutputOption | undefined,
): OutputOption | undefined {
  if (left === undefined) {
    return right;
  }
  if (right === undefined) {
    return left;
  }
  return left.index >= right.index ? left : right;
}

function looksLikeLinkerInput(token: string): boolean {
  if (token.length === 0 || token.startsWith("@")) {
    return false;
  }
  if (!token.startsWith("-") && !token.startsWith("/")) {
    return true;
  }
  return LINK_INPUT_SUFFIXES.has(fs.path.extension(token).toLowerCase());
}

/**
 * Interprets the argv remainder captured by a `cl`-style `/link` option.
 *
 * This stays deliberately narrow: it only recognizes linker switches that
 * affect the high-level command model and forwards plausible file-like tokens
 * as link inputs.
 *
 * @example
 * ```ts
 * // "/link /dll /out:bin/tool.dll foo.obj bar.res"
 * // becomes { shared: true, output: "bin/tool.dll", inputs: ["foo.obj", "bar.res"] }
 * ```
 */
function scanClLinkerRemainder(values: string[]) {
  let output: string | undefined;
  let shared = false;
  const inputs: string[] = [];

  for (let idx = 0; idx < values.length; ++idx) {
    const token = values[idx];
    const lower = token.toLowerCase();

    if (lower === "/dll") {
      shared = true;
      continue;
    }

    if (lower.startsWith("/out:")) {
      output = token.slice(5);
      continue;
    }

    if (lower === "/out" && idx + 1 < values.length) {
      output = values[idx + 1];
      ++idx;
      continue;
    }

    if (looksLikeLinkerInput(token)) {
      inputs.push(token);
    }
  }

  return {
    output,
    shared,
    inputs,
  };
}

function createDefaultModel(compiler: CompilerExe): CommandModel {
  return {
    compiler,
    style: "gnu",
    phase: CompilerPhaseValue.Link,
    artifact: CompilerArtifactValue.Executable,
    inputs: [],
    outputs: {},
  };
}

function setCompileResult(
  model: CommandModel,
  artifact: CompilerArtifact,
): void {
  model.phase = CompilerPhaseValue.Compile;
  model.artifact = artifact;
}

function setLinkResult(
  model: CommandModel,
  artifact:
    | typeof CompilerArtifactValue.Executable
    | typeof CompilerArtifactValue.SharedLibrary,
): void {
  model.phase = CompilerPhaseValue.Link;
  model.artifact = artifact;
}

function setArchiveResult(model: CommandModel): void {
  model.phase = CompilerPhaseValue.Archive;
  model.artifact = CompilerArtifactValue.StaticLibrary;
}

function setRelocatableLinkResult(model: CommandModel): void {
  model.phase = CompilerPhaseValue.RelocatableLink;
  model.artifact = CompilerArtifactValue.Object;
}

/**
 * Returns whether a parsed option consumes the rest of the argv in `cl` mode.
 *
 * `/link` is represented by LLVM as one option whose `values` already contain
 * the linker remainder, so fallback scanning must treat those argv slots as
 * consumed.
 *
 * @example
 * ```ts
 * // true for the parsed "/link /dll foo.obj" option item
 * ```
 */
function consumesClLinkRemainder(parsedItem: ParsedOption): boolean {
  return parsedItem.raw.id === ClangID.ID__SLASH_link;
}

function consumedArgCount(
  args: readonly string[],
  item: OptionItem,
  info: OptionInfo,
): number {
  switch (info.kind) {
    case OptionKindClass.GroupClass:
    case OptionKindClass.InputClass:
    case OptionKindClass.UnknownClass:
    case OptionKindClass.FlagClass:
    case OptionKindClass.JoinedClass:
    case OptionKindClass.RemainingArgsJoinedClass:
    case OptionKindClass.CommaJoinedClass:
      return 1;
    case OptionKindClass.ValuesClass:
    case OptionKindClass.SeparateClass:
    case OptionKindClass.RemainingArgsClass:
    case OptionKindClass.MultiArgClass:
      return 1 + item.values.length;
    case OptionKindClass.JoinedOrSeparateClass:
      return args[item.index] === item.key ? 1 + item.values.length : 1;
    case OptionKindClass.JoinedAndSeparateClass:
      return item.values.length === 0 ? 1 : item.values.length;
    default:
      return 1;
  }
}

function lastPrimaryOutputArg(args: readonly string[]): OutputArg | undefined {
  let found: OutputArg | undefined;

  for (let index = 0; index < args.length; ++index) {
    const token = args[index];
    if (token === "-o") {
      if (index + 1 >= args.length) {
        continue;
      }
      found = {
        value: args[index + 1],
        optionIndex: index,
        valueIndex: index + 1,
      };
      ++index;
      continue;
    }

    if (token.startsWith("-o") && token.length > 2) {
      found = {
        value: token.slice(2),
        optionIndex: index,
        valueIndex: index,
      };
    }
  }

  return found;
}

function removeInputAt(model: CommandModel, index: number, path: string): void {
  model.inputs = model.inputs.filter(
    (input) => !(input.index === index && input.path === path),
  );
}

/**
 * Computes which raw argv positions have already been consumed by opttable parsing.
 *
 * The fallback pass uses this to avoid reinterpreting arguments that LLVM has
 * already attached to a parsed option.
 *
 * @example
 * ```ts
 * // For ["--driver-mode=cl", "/link", "/dll", "foo.obj"],
 * // the returned set includes the "/link" slot and its remainder slots.
 * ```
 */
function collectConsumedArgIndexes(
  args: readonly string[],
  parsed: ParsedOption[],
): Set<number> {
  const indexes = new Set<number>();

  for (const parsedItem of parsed) {
    const count = consumesClLinkRemainder(parsedItem)
      ? 1 + parsedItem.raw.values.length
      : consumedArgCount(args, parsedItem.raw, parsedItem.rawInfo);
    for (let offset = 0; offset < count; ++offset) {
      indexes.add(parsedItem.raw.index + offset);
    }
  }

  return indexes;
}

function hasRecordedInput(
  model: CommandModel,
  index: number,
  path: string,
): boolean {
  return model.inputs.some(
    (input) => input.index === index && input.path === path,
  );
}

function isFallbackInputToken(token: string): boolean {
  if (token === "-") {
    return true;
  }
  if (token.startsWith("@")) {
    return false;
  }
  if (!token.startsWith("-")) {
    return true;
  }
  return os.platform() !== "windows" && token.startsWith("/");
}

/**
 * Applies fallback semantics for one argv token that opttable parsing did not consume.
 *
 * This is intentionally a second phase: the main analyzer trusts parsed options
 * first, then this helper only fills gaps for tokens that remain unclaimed.
 *
 * @example
 * ```ts
 * // Handles fallback forms such as "-o out.o" or a positional source file.
 * ```
 */
function applyClangLikeFallbackToken(
  model: CommandModel,
  args: readonly string[],
  consumedIndexes: Set<number>,
  index: number,
  positionalOnly: boolean,
): boolean {
  const token = args[index];

  if (positionalOnly) {
    if (!hasRecordedInput(model, index, token)) {
      recordInput(
        model,
        token,
        classifyInputKind(token, model.explicitLanguage),
        index,
      );
    }
    return positionalOnly;
  }

  switch (token) {
    case "--":
      return true;
    case "-c":
      setCompileResult(model, CompilerArtifactValue.Object);
      return false;
    case "-S":
      setCompileResult(
        model,
        model.artifact === CompilerArtifactValue.LlvmBitcode
          ? CompilerArtifactValue.LlvmIR
          : CompilerArtifactValue.Assembly,
      );
      return false;
    case "-E":
      model.phase = CompilerPhaseValue.Preprocess;
      model.artifact = CompilerArtifactValue.Stdout;
      return false;
    case "-fsyntax-only":
      model.phase = CompilerPhaseValue.SyntaxOnly;
      model.artifact = CompilerArtifactValue.None;
      return false;
    case "-emit-llvm":
    case "-emit-llvm-bc":
      setCompileResult(
        model,
        model.artifact === CompilerArtifactValue.Assembly
          ? CompilerArtifactValue.LlvmIR
          : CompilerArtifactValue.LlvmBitcode,
      );
      return false;
    case "-emit-pch":
      setCompileResult(model, CompilerArtifactValue.Pch);
      return false;
    case "-emit-module":
    case "-emit-module-interface":
    case "-emit-reduced-module-interface":
      setCompileResult(model, CompilerArtifactValue.Pcm);
      return false;
    case "-shared":
      setLinkResult(model, CompilerArtifactValue.SharedLibrary);
      return false;
    case "-r":
      setRelocatableLinkResult(model);
      return false;
    case "-x":
      if (index + 1 < args.length && !consumedIndexes.has(index + 1)) {
        model.explicitLanguage = args[index + 1];
        consumedIndexes.add(index + 1);
      }
      return false;
    case "-o":
      if (index + 1 < args.length && !consumedIndexes.has(index + 1)) {
        recordOutput(model, "primary", args[index + 1], index);
        consumedIndexes.add(index + 1);
      }
      return false;
  }

  if (token.startsWith("-x") && token.length > 2) {
    model.explicitLanguage = token.slice(2);
    return false;
  }

  if (token.startsWith("-o") && token.length > 2) {
    recordOutput(model, "primary", token.slice(2), index);
    return false;
  }

  if (isFallbackInputToken(token) && !hasRecordedInput(model, index, token)) {
    recordInput(
      model,
      token,
      classifyInputKind(token, model.explicitLanguage),
      index,
    );
  }

  return false;
}

/**
 * Replays lightweight fallback parsing over argv slots that opttable parsing left untouched.
 *
 * This preserves existing behavior for partially modeled flags without letting
 * fallback logic double-consume parsed options such as `cl`-style `/link`.
 *
 * @example
 * ```ts
 * // If "-o" was not present in parsed options, fallback can still record its output.
 * ```
 */
function applyClangLikeDriverFallbacks(
  model: CommandModel,
  args: readonly string[],
  parsed: ParsedOption[],
): void {
  const consumedIndexes = collectConsumedArgIndexes(args, parsed);
  let positionalOnly = false;

  for (let index = 0; index < args.length; ++index) {
    if (consumedIndexes.has(index)) {
      continue;
    }

    positionalOnly = applyClangLikeFallbackToken(
      model,
      args,
      consumedIndexes,
      index,
      positionalOnly,
    );

    if (
      (args[index] === "-x" || args[index] === "-o") &&
      consumedIndexes.has(index + 1)
    ) {
      ++index;
    }
  }
}

function repairPrimaryOutput(
  model: CommandModel,
  args: readonly string[],
): void {
  if (model.outputs.primary !== undefined) {
    return;
  }

  const output = lastPrimaryOutputArg(args);
  if (output === undefined) {
    return;
  }

  recordOutput(model, "primary", output.value, output.optionIndex);
  removeInputAt(model, output.valueIndex, output.value);
}

function resolveCompilerFamily(compiler: CompilerExe): CompilerFamily {
  switch (compiler) {
    case "clang":
    case "gcc":
      return "clang-like";
  }
}

function analyzeCompilerFamily(
  cmd: readonly string[],
  compiler: CompilerExe,
): CommandModel {
  const family = resolveCompilerFamily(compiler);

  switch (family) {
    case "clang-like":
      return analyzeClangLikeCommand(cmd, compiler);
    case "msvc-like":
    case "nvcc-like":
      throw new Error(`unsupported compiler family: ${family}`);
  }
}

/**
 * Marks the command as `cl`-style when the raw parsed option spelling proves it.
 *
 * This relies on opttable metadata instead of reparsing strings by hand.
 *
 * @example
 * ```ts
 * // "/c" or any option carrying CLOption visibility flips the style to "cl".
 * ```
 */
function observeClStyle(
  model: CommandModel,
  parsedItem: ParsedOption,
  allowClStyle: boolean,
): void {
  if (!allowClStyle) {
    return;
  }
  if (
    parsedItem.raw.key.startsWith("/") ||
    (parsedItem.rawInfo.visibility & ClangVisibility.CLOption) !== 0
  ) {
    model.style = "cl";
  }
}

/**
 * Applies the semantic effect of one opttable-parsed clang-like option.
 *
 * The goal is to keep all first-class option handling in one place before any
 * fallback scan of unconsumed argv slots happens.
 *
 * @example
 * ```ts
 * // "-c" selects object compilation, "/Tp file" records a source input,
 * // and "/link ..." forwards its parsed remainder to the linker scanner.
 * ```
 */
function applyParsedClangLikeOption(
  model: CommandModel,
  parsedItem: ParsedOption,
  allowClStyle: boolean,
): void {
  switch (parsedItem.item.id as ClangID) {
    case ClangID.ID_driver_mode:
      if (allowClStyle && parsedItem.item.values[0]?.toLowerCase() === "cl") {
        model.style = "cl";
      }
      break;
    case ClangID.ID_c:
    case ClangID.ID_emit_obj:
      setCompileResult(model, CompilerArtifactValue.Object);
      break;
    case ClangID.ID_S:
      setCompileResult(
        model,
        model.artifact === CompilerArtifactValue.LlvmBitcode
          ? CompilerArtifactValue.LlvmIR
          : CompilerArtifactValue.Assembly,
      );
      break;
    case ClangID.ID_E:
      model.phase = CompilerPhaseValue.Preprocess;
      model.artifact = CompilerArtifactValue.Stdout;
      break;
    case ClangID.ID_fsyntax_only:
      model.phase = CompilerPhaseValue.SyntaxOnly;
      model.artifact = CompilerArtifactValue.None;
      break;
    case ClangID.ID_emit_llvm:
    case ClangID.ID_emit_llvm_bc:
      setCompileResult(
        model,
        model.artifact === CompilerArtifactValue.Assembly
          ? CompilerArtifactValue.LlvmIR
          : CompilerArtifactValue.LlvmBitcode,
      );
      break;
    case ClangID.ID_emit_pch:
      setCompileResult(model, CompilerArtifactValue.Pch);
      break;
    case ClangID.ID_emit_module:
    case ClangID.ID_emit_module_interface:
    case ClangID.ID_emit_reduced_module_interface:
      setCompileResult(model, CompilerArtifactValue.Pcm);
      break;
    case ClangID.ID_emit_static_lib:
      setArchiveResult(model);
      break;
    case ClangID.ID_shared:
    case ClangID.ID__SLASH_LD:
    case ClangID.ID__SLASH_LDd:
      setLinkResult(model, CompilerArtifactValue.SharedLibrary);
      break;
    case ClangID.ID_r:
      setRelocatableLinkResult(model);
      break;
    case ClangID.ID_o:
    case ClangID.ID__SLASH_o:
      recordOutput(
        model,
        "primary",
        parsedItem.item.values[0],
        parsedItem.item.index,
      );
      break;
    case ClangID.ID__SLASH_Fo:
      recordOutput(
        model,
        "object",
        parsedItem.item.values[0],
        parsedItem.item.index,
      );
      break;
    case ClangID.ID__SLASH_Fe:
      recordOutput(
        model,
        "executable",
        parsedItem.item.values[0],
        parsedItem.item.index,
      );
      break;
    case ClangID.ID_x:
      model.explicitLanguage = parsedItem.item.values[0];
      break;
    case ClangID.ID__SLASH_TC:
      model.explicitLanguage = "c";
      break;
    case ClangID.ID__SLASH_TP:
      model.explicitLanguage = "c++";
      break;
    case ClangID.ID__SLASH_Tc:
    case ClangID.ID__SLASH_Tp:
      recordInput(
        model,
        parsedItem.item.values[0],
        "source",
        parsedItem.item.index,
      );
      break;
    case ClangID.ID__SLASH_link: {
      const linkerScan = scanClLinkerRemainder(parsedItem.item.values);
      recordOutput(model, "linker", linkerScan.output, parsedItem.item.index);
      if (linkerScan.shared) {
        setLinkResult(model, CompilerArtifactValue.SharedLibrary);
      }
      for (const input of linkerScan.inputs) {
        recordInput(model, input, "link", parsedItem.item.index);
      }
      break;
    }
    case ClangID.ID_INPUT:
      recordInput(
        model,
        parsedItem.item.key,
        classifyInputKind(parsedItem.item.key, model.explicitLanguage),
        parsedItem.item.index,
      );
      break;
    default:
      if (
        parsedItem.info.group === ClangID.ID_Action_Group &&
        model.phase === CompilerPhaseValue.Link &&
        model.artifact === CompilerArtifactValue.Executable
      ) {
        setCompileResult(model, CompilerArtifactValue.Unknown);
      }
      break;
  }
}

const analyzeClangLikeCommand: CompilerFamilyAnalyzer = (cmd, compiler) => {
  const model = createDefaultModel(compiler);
  const args = cmd.slice(1);
  const parsed = collectParsedOptions(
    "clang",
    args,
    clangLikeDriverVisibility(),
  );
  const allowClStyle = supportsClStyleAnalysis();

  for (const parsedItem of parsed) {
    observeClStyle(model, parsedItem, allowClStyle);
    applyParsedClangLikeOption(model, parsedItem, allowClStyle);
  }

  applyClangLikeDriverFallbacks(model, args, parsed);
  repairPrimaryOutput(model, args);
  return model;
};

function defaultExtensionForArtifact(
  style: CompilerStyle,
  artifact: CompilerArtifact,
): string | undefined {
  switch (artifact) {
    case CompilerArtifactValue.Object:
      return STYLE_DEFAULT_EXTENSIONS[style].object;
    case CompilerArtifactValue.Executable:
      return STYLE_DEFAULT_EXTENSIONS[style].executable;
    case CompilerArtifactValue.SharedLibrary:
      return STYLE_DEFAULT_EXTENSIONS[style].shared;
    case CompilerArtifactValue.StaticLibrary:
      return STYLE_DEFAULT_EXTENSIONS[style].staticLibrary;
    case CompilerArtifactValue.Assembly:
      return ".s";
    case CompilerArtifactValue.LlvmIR:
      return ".ll";
    case CompilerArtifactValue.LlvmBitcode:
      return ".bc";
    case CompilerArtifactValue.Pch:
      return ".pch";
    case CompilerArtifactValue.Pcm:
      return ".pcm";
    case CompilerArtifactValue.Ptx:
      return ".ptx";
    case CompilerArtifactValue.Cubin:
      return ".cubin";
    case CompilerArtifactValue.Fatbin:
      return ".fatbin";
    default:
      return undefined;
  }
}

function usesPerInputOutputs(
  phase: CompilerPhase,
  artifact: CompilerArtifact,
): boolean {
  if (phase !== CompilerPhaseValue.Compile) {
    return false;
  }

  switch (artifact) {
    case CompilerArtifactValue.Object:
    case CompilerArtifactValue.Assembly:
    case CompilerArtifactValue.LlvmIR:
    case CompilerArtifactValue.LlvmBitcode:
    case CompilerArtifactValue.Pch:
    case CompilerArtifactValue.Pcm:
    case CompilerArtifactValue.Ptx:
    case CompilerArtifactValue.Cubin:
    case CompilerArtifactValue.Fatbin:
      return true;
    default:
      return false;
  }
}

function sourceInputsOf(model: CommandModel): CompilerInput[] {
  return model.inputs.filter((input) => input.kind === "source");
}

function resolveDefaultCompileInputs(model: CommandModel): CompilerInput[] {
  const sourceInputs = sourceInputsOf(model);
  return sourceInputs.length > 0 ? sourceInputs : model.inputs;
}

function selectPreferredOutput(model: CommandModel): OutputOption | undefined {
  switch (model.phase) {
    case CompilerPhaseValue.Preprocess:
      return model.outputs.primary;
    case CompilerPhaseValue.SyntaxOnly:
      return undefined;
    case CompilerPhaseValue.Compile:
      return pickLaterOutput(model.outputs.primary, model.outputs.object);
    case CompilerPhaseValue.Link:
      return pickLaterOutput(
        pickLaterOutput(model.outputs.primary, model.outputs.executable),
        model.outputs.linker,
      );
    case CompilerPhaseValue.Archive:
    case CompilerPhaseValue.RelocatableLink:
    case CompilerPhaseValue.DeviceLink:
      return pickLaterOutput(
        pickLaterOutput(model.outputs.primary, model.outputs.object),
        model.outputs.linker,
      );
  }
}

function resolveOutputs(model: CommandModel): string[] {
  if (model.artifact === CompilerArtifactValue.None) {
    return [];
  }

  const preferredOutput = selectPreferredOutput(model);
  const defaultExt = defaultExtensionForArtifact(model.style, model.artifact);

  if (preferredOutput !== undefined) {
    if (
      endsWithPathSep(preferredOutput.value) &&
      defaultExt !== undefined &&
      model.inputs.length > 0
    ) {
      if (usesPerInputOutputs(model.phase, model.artifact)) {
        return resolveDefaultCompileInputs(model).map((input) =>
          fs.path.lexicalNormal(
            fs.path.joinAll(
              preferredOutput.value,
              pathStem(input.path) + defaultExt,
            ),
          ),
        );
      }

      return [
        fs.path.lexicalNormal(
          fs.path.joinAll(
            preferredOutput.value,
            pathStem(model.inputs[0].path) + defaultExt,
          ),
        ),
      ];
    }

    return [preferredOutput.value];
  }

  if (!usesPerInputOutputs(model.phase, model.artifact)) {
    return [];
  }
  if (defaultExt === undefined) {
    return [];
  }

  return resolveDefaultCompileInputs(model).map(
    (input) => pathStem(input.path) + defaultExt,
  );
}

function resolveLegacyCmdType(model: CommandModel): CompilerType | undefined {
  const hasSourceInput = sourceInputsOf(model).length > 0;

  switch (model.phase) {
    case CompilerPhaseValue.Preprocess:
      return LegacyTypeValue.SourcePreprocess;
    case CompilerPhaseValue.SyntaxOnly:
      return LegacyTypeValue.SourceSyntaxOnly;
    case CompilerPhaseValue.Compile:
      switch (model.artifact) {
        case CompilerArtifactValue.Object:
          return LegacyTypeValue.SourceToObject;
        case CompilerArtifactValue.Assembly:
          return LegacyTypeValue.SourceToAsm;
        case CompilerArtifactValue.LlvmIR:
          return LegacyTypeValue.SourceToLlvmIR;
        case CompilerArtifactValue.LlvmBitcode:
          return LegacyTypeValue.SourceToLlvmBC;
        case CompilerArtifactValue.Pch:
          return LegacyTypeValue.SourceToPch;
        case CompilerArtifactValue.Pcm:
          return LegacyTypeValue.SourceToPcm;
        default:
          return undefined;
      }
    case CompilerPhaseValue.Link:
      switch (model.artifact) {
        case CompilerArtifactValue.SharedLibrary:
          return LegacyTypeValue.ObjectToShare;
        case CompilerArtifactValue.Executable:
          return hasSourceInput
            ? LegacyTypeValue.SourceToExe
            : LegacyTypeValue.ObjectToExe;
        default:
          return undefined;
      }
    case CompilerPhaseValue.Archive:
      return LegacyTypeValue.ObjectToLib;
    case CompilerPhaseValue.RelocatableLink:
      return LegacyTypeValue.RelocatableLink;
    case CompilerPhaseValue.DeviceLink:
      return undefined;
  }
}

function resolveCompilerEdges(model: CommandModel, outputs: string[]): Edge[] {
  if (outputs.length === 0) {
    return [];
  }

  if (usesPerInputOutputs(model.phase, model.artifact)) {
    const compileInputs = resolveDefaultCompileInputs(model);
    if (outputs.length === compileInputs.length) {
      return outputs.map((output, index) => ({
        output,
        inputs: [compileInputs[index].path],
      }));
    }

    if (outputs.length === 1 && compileInputs.length === 1) {
      return [
        {
          output: outputs[0],
          inputs: [compileInputs[0].path],
        },
      ];
    }

    return [];
  }

  const consume = model.inputs.map((input) => input.path);
  return outputs.map((output) => ({
    output,
    inputs: [...consume],
  }));
}

function isCompilerCommand(cmd: readonly string[]): boolean {
  if (cmd.length === 0) {
    return false;
  }

  const compilerId = identify_compiler(cmd[0]);
  switch (compilerId) {
    case "clang":
    case "gcc":
      return true;
    default:
      return false;
  }
}

function analyzeCompilerModel(
  cmd: readonly string[],
): CommandModel | undefined {
  if (!isCompilerCommand(cmd)) {
    return undefined;
  }

  const compiler = identify_compiler(cmd[0]) as CompilerExe;
  return analyzeCompilerFamily(cmd, compiler);
}

export class CompilerAnalysis extends Analysis<CompilerExe> {
  /**
   * Stable registry key for the compiler analyzer.
   *
   * @example
   * ```ts
   * cmd.defaultRegistry.unregister(cmd.CompilerAnalysis.key);
   * ```
   */
  static readonly key = "compiler";

  /**
   * Checks whether a command looks like a supported compiler driver command.
   *
   * @example
   * ```ts
   * const ok = cmd.CompilerAnalysis.supports(["gcc", "-c", "main.c"]);
   * ```
   */
  static supports(cmd: readonly string[]): boolean {
    return isCompilerCommand(cmd);
  }

  /**
   * Analyzes a compiler command.
   *
   * @example
   * ```ts
   * const analysis = cmd.CompilerAnalysis.analyze(["clang", "-c", "main.c"]);
   * ```
   */
  static analyze(cmd: readonly string[]): CompilerAnalysis | undefined {
    return CompilerAnalysis.supports(cmd)
      ? new CompilerAnalysis(cmd)
      : undefined;
  }

  /**
   * Narrows a generic analysis back to a compiler analysis.
   *
   * @example
   * ```ts
   * const analysis = cmd.CompilerAnalysis.from(cmd.analyze(["clang", "-c", "main.c"]));
   * ```
   */
  static from(analysis: AnyAnalysis | undefined): CompilerAnalysis | undefined {
    return analysis instanceof CompilerAnalysis ? analysis : undefined;
  }

  /** Normalized compiler executable identifier. */
  readonly compiler: CompilerExe;
  /** High-level phase selected by the command. */
  readonly phase: CompilerPhase;
  /** Primary artifact kind implied by the command. */
  readonly artifact: CompilerArtifact;
  /** Legacy compatibility classification derived from `phase` and `artifact`. */
  readonly type: CompilerType | undefined;
  /** Driver syntax style used while parsing the command. */
  readonly style: CompilerStyle;
  /** Parsed inputs with source/link classification and argv position. */
  readonly inputItems: CompilerInput[];

  private readonly edgeList: Edge[];

  /**
   * Creates a compiler analysis from raw argv.
   *
   * @example
   * ```ts
   * const analysis = new cmd.CompilerAnalysis(["gcc", "-c", "main.c"]);
   * ```
   */
  constructor(cmd: readonly string[]) {
    const model = analyzeCompilerModel(cmd);
    if (model === undefined) {
      throw new Error("compiler command analysis required");
    }

    const produce = resolveOutputs(model);
    super(
      model.compiler,
      model.inputs.map((input) => input.path),
      produce,
    );

    this.compiler = model.compiler;
    this.phase = model.phase;
    this.artifact = model.artifact;
    this.type = resolveLegacyCmdType(model);
    this.style = model.style;
    this.inputItems = model.inputs.map((input) => ({ ...input }));
    this.edgeList = resolveCompilerEdges(model, produce).map((entry) => ({
      output: entry.output,
      inputs: [...entry.inputs],
    }));
  }

  /**
   * Returns parsed input records including their inferred role.
   *
   * @example
   * ```ts
   * const inputs = new cmd.CompilerAnalysis(["clang", "-c", "main.c"]).inputEntries();
   * ```
   */
  inputEntries(): CompilerInput[] {
    return this.inputItems.map((input) => ({ ...input }));
  }

  /**
   * Returns only the input paths consumed by this command.
   *
   * @example
   * ```ts
   * const inputs = new cmd.CompilerAnalysis(["clang", "-c", "main.c"]).inputs();
   * ```
   */
  inputs(): string[] {
    return [...this.consume];
  }

  /**
   * Returns only the inputs classified as source files.
   *
   * @example
   * ```ts
   * const sources = new cmd.CompilerAnalysis(["clang", "-c", "main.c"]).sourceInputs();
   * ```
   */
  sourceInputs(): string[] {
    return this.inputItems
      .filter((input) => input.kind === "source")
      .map((input) => input.path);
  }

  /**
   * Returns produced output paths.
   *
   * @example
   * ```ts
   * const outputs = new cmd.CompilerAnalysis(["clang", "-c", "main.c"]).outputs();
   * ```
   */
  outputs(): string[] {
    return [...this.produce];
  }

  /**
   * Returns precise dependency edges derived from compiler semantics.
   *
   * For compile commands this can pair each object file with its own source
   * file instead of falling back to the whole `consume` set.
   *
   * @example
   * ```ts
   * const edges = new cmd.CompilerAnalysis(["clang", "-c", "main.c"]).edges();
   * ```
   */
  override edges(): Edge[] {
    return this.edgeList.map((entry) => ({
      output: entry.output,
      inputs: [...entry.inputs],
    }));
  }
}

const _compilerAnalyzerCheck: Analyzer<CompilerAnalysis> = CompilerAnalysis;
