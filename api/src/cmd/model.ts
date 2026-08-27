import type { Result } from "catter/neverthrow";

/**
 * Describes one concrete dependency edge produced by a command.
 *
 * @example
 * ```ts
 * const edge: Edge = {
 *   output: "app.o",
 *   inputs: ["app.c"],
 * };
 * ```
 */
export type Edge = {
  output: string;
  inputs: readonly string[];
};

/**
 * Minimal process data needed by command analyzers.
 *
 * `exe` is the captured executable path or name. `argv` is the full argument
 * vector as captured for the process, including the executable argument.
 */
export type AnalyzedData = {
  readonly exe: string;
  readonly argv: readonly string[];
};

export abstract class AnalysisError extends Error {
  abstract readonly kind: string;
  constructor(message: string) {
    super(message);
    this.name = new.target.name;
  }
}

/**
 * Shared process invocation data and file effects for one analyzed command.
 *
 * `exe` and `argv` preserve the command invocation facts used for analysis.
 * `reads` records the files the command reads, `writes` records the files it
 * writes, and `edges` refines that into explicit output-to-input mappings when
 * an analyzer knows more. Concrete analyses add their own fields and narrow
 * `kind` to a literal for discriminated-union narrowing.
 *
 * @example
 * ```ts
 * const analysis: Analysis = {
 *   kind: "toy",
 *   exe: "toy",
 *   argv: ["toy", "in.dat", "out.pkg"],
 *   reads: ["in.dat"],
 *   writes: ["out.pkg"],
 *   edges: [{ output: "out.pkg", inputs: ["in.dat"] }],
 * };
 * ```
 */
export interface Analysis {
  /** Discriminator used to narrow concrete analysis variants. */
  readonly kind: string;
  /** Executable path or name used for analysis. */
  readonly exe: string;
  /** Full argument vector used for analysis. */
  readonly argv: readonly string[];
  /** Files read by the command. */
  readonly reads: readonly string[];
  /** Files written by the command. */
  readonly writes: readonly string[];
  /** Explicit output-to-input dependency edges for this analysis. */
  readonly edges: readonly Edge[];
}

/**
 * Consumer-side analyzer contract.
 *
 * Structural view of an analyzer with typed analysis and error results.
 * `Registry` stores this contract, so registered analyzers may be duck-typed
 * implementations rather than `Analyzer` subclasses.
 */
export interface IAnalyzer<
  T extends Analysis = Analysis,
  R extends AnalysisError = AnalysisError,
> {
  /** Stable identity of the analyzer, used for diagnostics and reporting. */
  readonly kind: string;
  /** Performs analysis and returns a typed result when successful. */
  analyze(command: AnalyzedData): Result<T, R>;
}

/**
 * Implementation-side analyzer contract used by `Registry`.
 *
 * Concrete analyzers extend this class and provide a stable `kind` plus
 * `analyze`. It implements the consumer-side `IAnalyzer` contract, so
 * instances are directly registrable.
 *
 * @example
 * ```ts
 * class ToyAnalysisError extends AnalysisError {
 *   readonly kind = "toy" as const;
 * }
 *
 * class ToyAnalysis implements Analysis {
 *   readonly kind = "toy" as const;
 *   readonly exe: string;
 *   readonly argv: readonly string[];
 *   readonly reads: readonly string[];
 *   readonly writes: readonly string[];
 *   readonly edges: readonly Edge[];
 *
 *   constructor(command: AnalyzedData, input: string, output: string) {
 *     this.exe = command.exe;
 *     this.argv = command.argv;
 *     this.reads = [input];
 *     this.writes = [output];
 *     this.edges = [{ output, inputs: [input] }];
 *   }
 * }
 *
 * class ToyAnalyzer extends Analyzer {
 *   readonly kind = "toy" as const;
 *
 *   analyze(command: AnalyzedData) {
 *     if (command.exe === "toy") {
 *       return neverthrow.ok(new ToyAnalysis(command, "in.dat", "out.pkg"));
 *     }
 *     return neverthrow.err(new ToyAnalysisError("not a toy command"));
 *   }
 * }
 * ```
 */
export abstract class Analyzer implements IAnalyzer<Analysis, AnalysisError> {
  abstract readonly kind: string;
  /** Performs analysis and returns a typed result when successful. */
  abstract analyze(command: AnalyzedData): Result<Analysis, AnalysisError>;
}
