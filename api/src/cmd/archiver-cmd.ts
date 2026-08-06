import { fromThrowable, type Result } from "catter/neverthrow";
import { Analysis, AnalysisError, Analyzer, AnalyzedData } from "./model.js";

export class ArchiverNotRecognizedError extends AnalysisError {
  readonly kind = "archiver-not-recognized" as const;
}

export class ArchiverUnsupportedError extends AnalysisError {
  readonly kind = "archiver-unsupported" as const;
}

export class ArchiverParseError extends AnalysisError {
  readonly kind = "archiver-parse" as const;
}

export type ArchiverAnalysisError =
  | ArchiverNotRecognizedError
  | ArchiverUnsupportedError
  | ArchiverParseError;

function toArchiverAnalysisError(
  value: unknown,
  context: string,
): ArchiverAnalysisError {
  if (
    value instanceof ArchiverNotRecognizedError ||
    value instanceof ArchiverUnsupportedError ||
    value instanceof ArchiverParseError
  ) {
    return value;
  }

  if (value instanceof Error) {
    return new ArchiverParseError(`${context}: ${value.message}`);
  }

  return new ArchiverParseError(`${context}: ${String(value)}`);
}

/**
 * Supported single-letter archive operations.
 *
 * @example
 * ```ts
 * const op = cmd.ArchiverOperation.ReplaceOrInsert;
 * ```
 */
export const ArchiverOperation = {
  Delete: "d",
  Move: "m",
  Print: "p",
  QuickAppend: "q",
  ReplaceOrInsert: "r",
  SymbolTable: "s",
  Table: "t",
  Extract: "x",
} as const;

/**
 * Union type of supported archive operations.
 *
 * @example
 * ```ts
 * const op: cmd.ArchiverOperation = cmd.ArchiverOperation.QuickAppend;
 * ```
 */
export type ArchiverOperation =
  (typeof ArchiverOperation)[keyof typeof ArchiverOperation];

/**
 * Recognized archiver executable identifiers.
 *
 * @example
 * ```ts
 * const exe: cmd.ArchiverExe = "llvm-ar";
 * ```
 */
export type ArchiverExe = "ar" | "llvm-ar" | "gcc-ar";

export type ArchiverModel = {
  operation: ArchiverOperation;
  modifiers: string[];
  thin: boolean;
  archive?: string;
  members: string[];
  scriptMode: boolean;
  reads: string[];
  writes: string[];
};

const ARCHIVER_EXE_NAMES = new Set<ArchiverExe>(["ar", "llvm-ar", "gcc-ar"]);
const ARCHIVER_OPERATION_SET = new Set<ArchiverOperation>([
  ArchiverOperation.Delete,
  ArchiverOperation.Move,
  ArchiverOperation.Print,
  ArchiverOperation.QuickAppend,
  ArchiverOperation.ReplaceOrInsert,
  ArchiverOperation.SymbolTable,
  ArchiverOperation.Table,
  ArchiverOperation.Extract,
]);
const PRIMARY_ARCHIVER_OPERATIONS = new Set<ArchiverOperation>([
  ArchiverOperation.Delete,
  ArchiverOperation.Move,
  ArchiverOperation.Print,
  ArchiverOperation.QuickAppend,
  ArchiverOperation.ReplaceOrInsert,
  ArchiverOperation.Table,
  ArchiverOperation.Extract,
]);
const MODELED_ARCHIVER_OPERATIONS = new Set<ArchiverOperation>([
  ArchiverOperation.Print,
  ArchiverOperation.QuickAppend,
  ArchiverOperation.ReplaceOrInsert,
  ArchiverOperation.Table,
]);

function exeStem(value: string): string {
  const slash = value.lastIndexOf("/");
  const backslash = value.lastIndexOf("\\");
  const index = Math.max(slash, backslash);
  const name = index === -1 ? value : value.slice(index + 1);
  if (name.toLowerCase().endsWith(".exe")) {
    return name.slice(0, -4).toLowerCase();
  }
  return name.toLowerCase();
}

function isArchiverExe(value: string): value is ArchiverExe {
  return ARCHIVER_EXE_NAMES.has(value as ArchiverExe);
}

function isOptionToken(token: string): boolean {
  return token.startsWith("--") || token.startsWith("@");
}

function parseOperationToken(
  token: string,
): { operation: ArchiverOperation; modifiers: string[] } | undefined {
  const stripped = token.startsWith("-") ? token.slice(1) : token;
  if (stripped.length === 0) {
    return undefined;
  }

  let operation: ArchiverOperation | undefined;
  const modifiers: string[] = [];

  for (const ch of stripped) {
    const value = ch as ArchiverOperation;
    if (PRIMARY_ARCHIVER_OPERATIONS.has(value)) {
      if (operation !== undefined) {
        return undefined;
      }
      operation = value;
      continue;
    }

    modifiers.push(ch);
  }

  if (operation === undefined) {
    if (stripped === ArchiverOperation.SymbolTable) {
      return {
        operation: ArchiverOperation.SymbolTable,
        modifiers: [],
      };
    }
    return undefined;
  }

  return {
    operation,
    modifiers,
  };
}

function analyzeArchiverModel(command: AnalyzedData): ArchiverModel {
  if (command.argv.length === 0) {
    throw new ArchiverNotRecognizedError("empty argv");
  }

  const exe = exeStem(command.exe);
  if (!isArchiverExe(exe)) {
    throw new ArchiverNotRecognizedError(
      `not a recognized archiver executable: ${command.exe}`,
    );
  }

  const cmd = command.argv;
  let index = 1;
  let thin = false;

  while (index < cmd.length) {
    const token = cmd[index];
    if (token === "-M") {
      throw new ArchiverUnsupportedError(
        "archiver MRI script mode is not supported",
      );
    }
    if (token === "--thin" || token === "-T") {
      thin = true;
      ++index;
      continue;
    }
    if (
      token.startsWith("--format=") ||
      token === "--format" ||
      token.startsWith("--plugin=") ||
      token === "--plugin" ||
      token.startsWith("--rsp-quoting=") ||
      token === "--rsp-quoting" ||
      token.startsWith("--output=") ||
      token === "--output" ||
      /^-X(32|64|32_64|any)$/.test(token) ||
      token === "--version" ||
      token === "--help" ||
      token === "-h"
    ) {
      if (
        (token === "--format" ||
          token === "--plugin" ||
          token === "--rsp-quoting" ||
          token === "--output") &&
        index + 1 < cmd.length
      ) {
        index += 2;
      } else {
        ++index;
      }
      continue;
    }
    if (isOptionToken(token)) {
      throw new ArchiverUnsupportedError(
        `unsupported archiver option: ${token}`,
      );
    }
    break;
  }

  if (index >= cmd.length) {
    throw new ArchiverUnsupportedError(
      "archiver command has no operation token",
    );
  }

  const parsedOperation = parseOperationToken(cmd[index]);
  if (parsedOperation === undefined) {
    throw new ArchiverUnsupportedError(
      `unsupported archiver operation token: ${cmd[index]}`,
    );
  }
  ++index;

  const archive = cmd[index];
  if (archive === undefined) {
    throw new ArchiverUnsupportedError("archiver command has no archive path");
  }
  ++index;

  if (!MODELED_ARCHIVER_OPERATIONS.has(parsedOperation.operation)) {
    throw new ArchiverUnsupportedError(
      `archiver operation is not modeled: ${parsedOperation.operation}`,
    );
  }

  const members = cmd.slice(index);
  const writes =
    parsedOperation.operation === ArchiverOperation.QuickAppend ||
    parsedOperation.operation === ArchiverOperation.ReplaceOrInsert
      ? [archive]
      : [];
  const reads =
    parsedOperation.operation === ArchiverOperation.Print ||
    parsedOperation.operation === ArchiverOperation.Table
      ? [archive]
      : [...members];

  return {
    operation: parsedOperation.operation,
    modifiers: parsedOperation.modifiers,
    thin,
    archive,
    members: [...members],
    scriptMode: false,
    reads,
    writes,
  };
}

export class ArchiverAnalysis extends Analysis {
  /** Discriminator for command analysis unions. */
  readonly kind = "archiver" as const;
  /** The parsed archive operation. */
  readonly operation: ArchiverOperation;
  /** Extra modifier letters attached to the operation token. */
  readonly modifiers: readonly string[];
  /** Whether thin-archive mode was requested. */
  readonly thin: boolean;
  /** Archive file path when the command syntax provides one. */
  readonly archive?: string;
  /** Member file paths listed after the archive path. */
  readonly members: readonly string[];
  /** Whether GNU MRI script mode was requested. */
  readonly scriptMode: boolean;

  constructor(model: ArchiverModel, command: AnalyzedData) {
    super({
      exe: command.exe,
      argv: command.argv,
      reads: model.reads,
      writes: model.writes,
      edges: model.writes.map((output) => ({
        output,
        inputs: [...model.reads],
      })),
    });
    this.operation = model.operation;
    this.modifiers = [...model.modifiers];
    this.thin = model.thin;
    this.archive = model.archive;
    this.members = [...model.members];
    this.scriptMode = model.scriptMode;
  }
}

export class ArchiverAnalyzer extends Analyzer {
  readonly kind = "archiver" as const;

  analyze(
    command: AnalyzedData,
  ): Result<ArchiverAnalysis, ArchiverAnalysisError> {
    return fromThrowable(
      () => new ArchiverAnalysis(analyzeArchiverModel(command), command),
      (error) => toArchiverAnalysisError(error, "archiver analysis failed"),
    )();
  }
}
