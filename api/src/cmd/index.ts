import type {
  ArchiverAnalysis,
  ArchiverAnalysisError,
} from "./archiver-cmd.js";
import type {
  CompilerAnalysis,
  CompilerAnalysisError,
} from "./compiler-cmd.js";

export * from "./model.js";
export * from "./registry.js";
export * from "./compiler-cmd.js";
export * from "./archiver-cmd.js";

/** Built-in command analysis result variants. */
export type CommandAnalysis = CompilerAnalysis | ArchiverAnalysis;

/** Built-in command analyzer error variants. */
export type CommandAnalyzerError =
  | CompilerAnalysisError
  | ArchiverAnalysisError;
