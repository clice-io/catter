import * as option from "catter/option";
import { ClangID, ClangVisibility } from "catter/option";
import { type OptionInfo, type OptionItem } from "catter/option";
import type {
  CompilerAction,
  CompilerDialect,
  CompilerInput,
  CompilerOutput,
  CompilerParseResult,
  CompilerTargetFact,
} from "../types.js";
import { CompilerParseError } from "../errors.js";
import { resolveCompilerMode } from "./compiler-mode.js";

export type ClangDriverParsedOption = {
  raw: OptionItem;
  item: OptionItem;
  info: OptionInfo;
};

type ClangDriverParserState = {
  dialect: CompilerDialect;
  target: CompilerTargetFact | undefined;
  explicitLanguage: string | undefined;
  compilerActions: CompilerAction[];
  inputCandidates: CompilerInput[];
  outputCandidates: CompilerOutput[];
  inputs: CompilerInput[];
  outputs: CompilerOutput[];
};

export const CLANG_CL_VISIBILITY =
  ClangVisibility.DefaultVis | ClangVisibility.CLOption;

export function collectClangDriverOptions(
  args: readonly string[],
  visibility: number = ClangVisibility.DefaultVis,
): ClangDriverParsedOption[] {
  const collected = option.collect("clang", [...args], visibility);
  if (!Array.isArray(collected)) {
    throw new CompilerParseError(`fatal error in parsing: ${collected}`);
  }

  return collected.map((raw) => {
    const item = {
      ...raw,
      values: [...raw.values],
    };
    return {
      raw,
      item,
      info: option.info("clang", item),
    };
  });
}

export function clangDriverOptionValue(
  parsedItem: ClangDriverParsedOption,
  valueIndex = 0,
): string {
  const value = parsedItem.item.values[valueIndex];
  if (value === undefined) {
    throw new CompilerParseError(
      `clang option ${parsedItem.raw.key} is missing value ${valueIndex}`,
    );
  }
  return value;
}

function applyParsedGnuClangDriverOption(
  state: ClangDriverParserState,
  parsedItem: ClangDriverParsedOption,
): void {
  switch (parsedItem.item.id as ClangID) {
    case ClangID.ID_c:
    case ClangID.ID_emit_obj:
      state.compilerActions.push({
        kind: "compile-object",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_S:
      state.compilerActions.push({
        kind: "compile-assembly-like",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_E:
      state.compilerActions.push({
        kind: "preprocess",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_fsyntax_only:
      state.compilerActions.push({
        kind: "syntax-only",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_emit_llvm:
    case ClangID.ID_emit_llvm_bc:
      state.compilerActions.push({
        kind: "compile-llvm-like",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_emit_pch:
      state.compilerActions.push({
        kind: "compile-pch",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_emit_module:
    case ClangID.ID_emit_module_interface:
    case ClangID.ID_emit_reduced_module_interface:
      state.compilerActions.push({
        kind: "compile-pcm",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_emit_static_lib:
      state.compilerActions.push({
        kind: "archive",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_shared:
      state.compilerActions.push({
        kind: "link-shared-library",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_r:
      state.compilerActions.push({
        kind: "relocatable-link",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_o:
      state.outputs.push({
        path: clangDriverOptionValue(parsedItem),
        kind: "primary-artifact",
        index: parsedItem.item.index,
        source: {
          kind: "option",
          option: parsedItem.raw.key,
          optionIndex: parsedItem.item.index,
        },
      });
      break;
    case ClangID.ID_target:
    case ClangID.ID_target_legacy_spelling:
      state.target = {
        target: { triple: clangDriverOptionValue(parsedItem) },
        source: {
          kind: "argument",
          option: parsedItem.raw.key,
          index: parsedItem.item.index,
        },
      };
      break;
    case ClangID.ID_x:
      state.explicitLanguage = clangDriverOptionValue(parsedItem);
      break;
    case ClangID.ID_INPUT:
      state.inputCandidates.push({
        path: parsedItem.item.key,
        index: parsedItem.item.index,
        source: { kind: "argument" },
        language: state.explicitLanguage,
      });
      break;
    default:
      if (parsedItem.info.group === ClangID.ID_Action_Group) {
        state.compilerActions.push({
          kind: "unknown-compile-action",
          index: parsedItem.item.index,
        });
      }
      break;
  }
}

function applyParsedClangClDriverOption(
  state: ClangDriverParserState,
  parsedItem: ClangDriverParsedOption,
): void {
  switch (parsedItem.item.id as ClangID) {
    case ClangID.ID_c:
      state.compilerActions.push({
        kind: "compile-object",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_E:
      state.compilerActions.push({
        kind: "preprocess",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_P:
    case ClangID.ID__SLASH_P:
      state.compilerActions.push({
        kind: "preprocess",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_fsyntax_only:
      state.compilerActions.push({
        kind: "syntax-only",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID__SLASH_LD:
    case ClangID.ID__SLASH_LDd:
      state.compilerActions.push({
        kind: "link-shared-library",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID_o:
    case ClangID.ID__SLASH_o:
      state.outputs.push({
        path: clangDriverOptionValue(parsedItem),
        kind: "primary-artifact",
        index: parsedItem.item.index,
        source: {
          kind: "option",
          option: parsedItem.raw.key,
          optionIndex: parsedItem.item.index,
        },
      });
      break;
    case ClangID.ID_target:
    case ClangID.ID_target_legacy_spelling:
      state.target = {
        target: { triple: clangDriverOptionValue(parsedItem) },
        source: {
          kind: "argument",
          option: parsedItem.raw.key,
          index: parsedItem.item.index,
        },
      };
      break;
    case ClangID.ID_x:
      state.explicitLanguage = clangDriverOptionValue(parsedItem);
      break;
    case ClangID.ID__SLASH_Fo:
      state.outputs.push({
        path: clangDriverOptionValue(parsedItem),
        kind: "object-file",
        index: parsedItem.item.index,
        source: {
          kind: "option",
          option: parsedItem.raw.key,
          optionIndex: parsedItem.item.index,
        },
      });
      break;
    case ClangID.ID__SLASH_Fe:
      state.outputs.push({
        path: clangDriverOptionValue(parsedItem),
        kind: "linked-artifact",
        index: parsedItem.item.index,
        source: {
          kind: "option",
          option: parsedItem.raw.key,
          optionIndex: parsedItem.item.index,
        },
      });
      break;
    case ClangID.ID__SLASH_FA:
      state.compilerActions.push({
        kind: "emit-assembly-listing",
        index: parsedItem.item.index,
      });
      break;
    case ClangID.ID__SLASH_Fa:
      if (parsedItem.item.values[0] === undefined) {
        state.compilerActions.push({
          kind: "emit-assembly-listing",
          index: parsedItem.item.index,
        });
      } else {
        state.compilerActions.push({
          kind: "emit-assembly-listing",
          index: parsedItem.item.index,
          path: parsedItem.item.values[0],
        });
      }
      break;
    case ClangID.ID__SLASH_TC:
      state.explicitLanguage = "c";
      break;
    case ClangID.ID__SLASH_TP:
      state.explicitLanguage = "c++";
      break;
    case ClangID.ID__SLASH_Tc:
      state.inputs.push({
        path: clangDriverOptionValue(parsedItem),
        index: parsedItem.item.index,
        source: {
          kind: "option",
          option: parsedItem.raw.key,
          optionIndex: parsedItem.item.index,
        },
        language: "c",
      });
      break;
    case ClangID.ID__SLASH_Tp: {
      state.inputs.push({
        path: clangDriverOptionValue(parsedItem),
        index: parsedItem.item.index,
        source: {
          kind: "option",
          option: parsedItem.raw.key,
          optionIndex: parsedItem.item.index,
        },
        language: "c++",
      });
      break;
    }
    case ClangID.ID__SLASH_link:
      applyTemporaryClangClLinkerRemainder(state, parsedItem);
      break;
    case ClangID.ID_INPUT:
      state.inputCandidates.push({
        path: parsedItem.item.key,
        index: parsedItem.item.index,
        source: { kind: "argument" },
        language: state.explicitLanguage,
      });
      break;
    default:
      if (parsedItem.info.group === ClangID.ID_Action_Group) {
        throw new CompilerParseError(
          `unsupported clang-cl action option ${parsedItem.raw.key}`,
        );
      }
      break;
  }
}

function applyTemporaryClangClLinkerRemainder(
  state: ClangDriverParserState,
  parsedItem: ClangDriverParsedOption,
): void {
  // Temporary COFF linker remainder extraction. Keep this narrow until the
  // command registry has a dedicated linker analyzer/parser for link.exe/lld-link.
  const values = parsedItem.item.values;
  for (let index = 0; index < values.length; ++index) {
    const token = values[index]!;
    const tokenIndex = parsedItem.item.index + 1 + index;
    const lower = token.toLowerCase();

    if (lower === "/dll") {
      state.compilerActions.push({
        kind: "link-shared-library",
        index: tokenIndex,
      });
      continue;
    }

    if (lower.startsWith("/out:")) {
      state.outputs.push({
        path: token.slice(5),
        kind: "linked-artifact",
        index: tokenIndex,
        source: {
          kind: "remainder-option",
          boundary: parsedItem.raw.key,
          boundaryIndex: parsedItem.item.index,
          option: token.slice(0, 5),
          optionIndex: tokenIndex,
        },
      });
      continue;
    }

    if (lower === "/out" && index + 1 < values.length) {
      state.outputs.push({
        path: values[index + 1]!,
        kind: "linked-artifact",
        index: tokenIndex + 1,
        source: {
          kind: "remainder-option",
          boundary: parsedItem.raw.key,
          boundaryIndex: parsedItem.item.index,
          option: token,
          optionIndex: tokenIndex,
        },
      });
      ++index;
      continue;
    }

    if (token.length === 0 || token.startsWith("@")) {
      continue;
    }

    if (token.startsWith("-") || token.startsWith("/")) {
      continue;
    }

    state.inputs.push({
      path: token,
      index: tokenIndex,
      source: {
        kind: "remainder-argument",
        boundary: parsedItem.raw.key,
        boundaryIndex: parsedItem.item.index,
      },
    });
  }
}

export function buildClangGnuDriverModel(
  parsed: ClangDriverParsedOption[],
  dialect: CompilerDialect,
): CompilerParseResult {
  const state: ClangDriverParserState = {
    dialect,
    target: undefined,
    explicitLanguage: undefined,
    compilerActions: [],
    inputCandidates: [],
    outputCandidates: [],
    inputs: [],
    outputs: [],
  };

  for (const parsedItem of parsed) {
    applyParsedGnuClangDriverOption(state, parsedItem);
  }

  return {
    dialect: state.dialect,
    target: state.target,
    compilerMode: resolveCompilerMode(state.dialect, state.compilerActions),
    compilerActions: state.compilerActions,
    inputCandidates: state.inputCandidates,
    outputCandidates: state.outputCandidates,
    inputs: state.inputs,
    outputs: state.outputs,
  };
}

export function buildClangClDriverModel(
  parsed: ClangDriverParsedOption[],
  dialect: CompilerDialect,
): CompilerParseResult {
  const state: ClangDriverParserState = {
    dialect,
    target: undefined,
    explicitLanguage: undefined,
    compilerActions: [],
    inputCandidates: [],
    outputCandidates: [],
    inputs: [],
    outputs: [],
  };

  for (const parsedItem of parsed) {
    applyParsedClangClDriverOption(state, parsedItem);
  }

  return {
    dialect: state.dialect,
    target: state.target,
    compilerMode: resolveCompilerMode(state.dialect, state.compilerActions),
    compilerActions: state.compilerActions,
    inputCandidates: state.inputCandidates,
    outputCandidates: state.outputCandidates,
    inputs: state.inputs,
    outputs: state.outputs,
  };
}
