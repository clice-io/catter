import type { Compiler } from "catter-c";
import { identify_compiler } from "catter-c";

import * as debug from "../debug.js";
import * as fs from "../fs.js";
import * as os from "../os.js";
import * as option from "../option/index.js";
import { ClangID, ClangVisibility } from "../option/clang.js";
import type { OptionInfo, OptionItem, OptionTable } from "../option/types.js";

export const CompilerCommandType = {
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
export type CompilerCommandType =
  (typeof CompilerCommandType)[keyof typeof CompilerCommandType];

export const CompilerPhase = {
  Preprocess: "preprocess",
  SyntaxOnly: "syntax-only",
  Compile: "compile",
  Link: "link",
  Archive: "archive",
  RelocatableLink: "relocatable-link",
  DeviceLink: "device-link",
} as const;
export type CompilerPhase = (typeof CompilerPhase)[keyof typeof CompilerPhase];

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
export type CompilerArtifact =
  (typeof CompilerArtifact)[keyof typeof CompilerArtifact];

type SupportedCompiler = Extract<Compiler, "clang" | "gcc">;
type CompilerStyleValue = "gnu" | "cl";

type CompilerFamily = "clang-like" | "msvc-like" | "nvcc-like";
type CommandInputKind = "source" | "link";
type OutputChannel = "primary" | "object" | "executable" | "linker";

type ParsedOption = {
  raw: OptionItem;
  rawInfo: OptionInfo;
  item: OptionItem;
  info: OptionInfo;
};

type CommandInput = {
  path: string;
  kind: CommandInputKind;
  index: number;
};

type OutputOption = {
  value: string;
  index: number;
  channel: OutputChannel;
};

type CommandModel = {
  compiler: SupportedCompiler;
  style: CompilerStyleValue;
  phase: CompilerPhaseValue;
  artifact: CompilerArtifactValue;
  explicitLanguage?: string;
  inputs: CommandInput[];
  outputs: Partial<Record<OutputChannel, OutputOption>>;
};

type CompilerFamilyAnalyzer = (
  cmd: string[],
  compiler: SupportedCompiler,
) => CommandModel;

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
  CompilerStyleValue,
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
  args: string[],
  visibility?: number,
): ParsedOption[] {
  const collected = option.collect(table, args, visibility);
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

function classifyPathBySuffix(value: string): CommandInputKind {
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
): CommandInputKind {
  const explicit = languageIsSource(explicitLanguage);
  if (explicit !== undefined) {
    return explicit ? "source" : "link";
  }
  return classifyPathBySuffix(value);
}

function recordInput(
  model: CommandModel,
  path: string | undefined,
  kind: CommandInputKind,
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

function createDefaultModel(compiler: SupportedCompiler): CommandModel {
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
  artifact: CompilerArtifactValue,
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

function resolveCompilerFamily(compiler: SupportedCompiler): CompilerFamily {
  switch (compiler) {
    case "clang":
    case "gcc":
      return "clang-like";
  }
}

function analyzeCompilerFamily(
  cmd: string[],
  compiler: SupportedCompiler,
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

const analyzeClangLikeCommand: CompilerFamilyAnalyzer = (cmd, compiler) => {
  const model = createDefaultModel(compiler);
  const parsed = collectParsedOptions(
    "clang",
    cmd.slice(1),
    clangLikeDriverVisibility(),
  );
  const allowClStyle = supportsClStyleAnalysis();

  for (const parsedItem of parsed) {
    if (
      (allowClStyle && parsedItem.raw.key.startsWith("/")) ||
      (allowClStyle &&
        (parsedItem.rawInfo.visibility & ClangVisibility.CLOption) !== 0)
    ) {
      model.style = "cl";
    }

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

  return model;
};

function defaultExtensionForArtifact(
  style: CompilerStyleValue,
  artifact: CompilerArtifactValue,
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
  phase: CompilerPhaseValue,
  artifact: CompilerArtifactValue,
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

function sourceInputsOf(model: CommandModel): CommandInput[] {
  return model.inputs.filter((input) => input.kind === "source");
}

function resolveDefaultCompileInputs(model: CommandModel): CommandInput[] {
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

function resolveLegacyCmdType(
  model: CommandModel,
): LegacyCmdTypeValue | undefined {
  const hasSourceInput = sourceInputsOf(model).length > 0;

  switch (model.phase) {
    case CompilerPhaseValue.Preprocess:
      return LegacyCmdTypeValue.SourcePreprocess;
    case CompilerPhaseValue.SyntaxOnly:
      return LegacyCmdTypeValue.SourceSyntaxOnly;
    case CompilerPhaseValue.Compile:
      switch (model.artifact) {
        case CompilerArtifactValue.Object:
          return LegacyCmdTypeValue.SourceToObject;
        case CompilerArtifactValue.Assembly:
          return LegacyCmdTypeValue.SourceToAsm;
        case CompilerArtifactValue.LlvmIR:
          return LegacyCmdTypeValue.SourceToLlvmIR;
        case CompilerArtifactValue.LlvmBitcode:
          return LegacyCmdTypeValue.SourceToLlvmBC;
        case CompilerArtifactValue.Pch:
          return LegacyCmdTypeValue.SourceToPch;
        case CompilerArtifactValue.Pcm:
          return LegacyCmdTypeValue.SourceToPcm;
        default:
          return undefined;
      }
    case CompilerPhaseValue.Link:
      switch (model.artifact) {
        case CompilerArtifactValue.SharedLibrary:
          return LegacyCmdTypeValue.ObjectToShare;
        case CompilerArtifactValue.Executable:
          return hasSourceInput
            ? LegacyCmdTypeValue.SourceToExe
            : LegacyCmdTypeValue.ObjectToExe;
        default:
          return undefined;
      }
    case CompilerPhaseValue.Archive:
      return LegacyCmdTypeValue.ObjectToLib;
    case CompilerPhaseValue.RelocatableLink:
      return LegacyCmdTypeValue.RelocatableLink;
    case CompilerPhaseValue.DeviceLink:
      return undefined;
  }
}

function isCompilationDatabaseCommand(model: CommandModel): boolean {
  return (
    model.phase === CompilerPhaseValue.Compile &&
    model.artifact === CompilerArtifactValue.Object &&
    sourceInputsOf(model).length > 0
  );
}

function resolveCompilationDatabaseEntries(
  model: CommandModel,
): Array<{ file: string; output?: string }> {
  if (!isCompilationDatabaseCommand(model)) {
    return [];
  }

  const sourceInputs = sourceInputsOf(model);
  const outputs = resolveOutputs(model);
  return sourceInputs.map((input, index) => {
    const entry: { file: string; output?: string } = {
      file: input.path,
    };

    if (outputs.length === sourceInputs.length) {
      entry.output = outputs[index];
    } else if (outputs.length === 1 && sourceInputs.length === 1) {
      entry.output = outputs[0];
    }

    return entry;
  });
}

export class CompilerCmdAnalysis {
  static isSupport(cmd: string[]): boolean {
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

  readonly compiler: "clang" | "gcc";
  readonly phase: CompilerPhase;
  readonly artifact: CompilerArtifact;
  readonly type: CompilerCommandType | undefined;
  readonly style: "gnu" | "cl";

  private readonly inputList: string[];
  private readonly compilationDatabaseEntryList: Array<{
    file: string;
    output?: string;
  }>;
  private readonly outputList: string[];

  constructor(cmd: string[]) {
    debug.assertThrow(CompilerCmdAnalysis.isSupport(cmd));

    this.compiler = identify_compiler(cmd[0]) as SupportedCompiler;

    const model = analyzeCompilerFamily(cmd, this.compiler);

    this.phase = model.phase;
    this.artifact = model.artifact;
    this.type = resolveLegacyCmdType(model);
    this.inputList = model.inputs.map((input) => input.path);
    this.compilationDatabaseEntryList =
      resolveCompilationDatabaseEntries(model);
    this.outputList = resolveOutputs(model);
    this.style = model.style;
  }

  inputs(): string[] {
    return [...this.inputList];
  }

  outputs(): string[] {
    return [...this.outputList];
  }

  compilationDatabaseEntries(): Array<{ file: string; output?: string }> {
    return this.compilationDatabaseEntryList.map((entry) => ({ ...entry }));
  }
}

const LegacyCmdTypeValue = CompilerCommandType;
type LegacyCmdTypeValue = CompilerCommandType;

const CompilerPhaseValue = CompilerPhase;
type CompilerPhaseValue = CompilerPhase;

const CompilerArtifactValue = CompilerArtifact;
type CompilerArtifactValue = CompilerArtifact;
