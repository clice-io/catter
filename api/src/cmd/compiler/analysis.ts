import { fromThrowable, type Result } from "catter/neverthrow";
import { Analyzer } from "../model.js";
import type { Analysis, AnalyzedData, Edge } from "../model.js";
import { CompilerAnalysisError, toCompilerAnalysisError } from "./errors.js";
import { CompilerIdentifier } from "./identify.js";
import { parseCompilerCommand } from "./parsers/index.js";
import { CompilerResolver } from "./resolver/index.js";
import type {
  CompilerAnalyzerOptions,
  CompilerParseResult,
  CompilerResolveResult,
  CompilerMode,
  UnwrappedCompilerCommand,
  CompilerResolveDebug,
  EffectiveCompilerTarget,
} from "./types.js";
import { unwrapCompilerCommand } from "./unwrap.js";

/**
 * Analysis result for a recognized compiler driver invocation.
 *
 * This is driver-level analysis, not compile-only analysis. A compiler driver
 * can preprocess, compile, link, archive, or dispatch another tool; the
 * `compilerMode` field describes the high-level action inferred for this
 * invocation. The base `Analysis` fields expose the logical file effects that
 * the driver command is expected to produce: `reads`, `writes`, and `edges`.
 * Compiler-specific fields describe how the command was identified and parsed.
 */
export class CompilerAnalysis implements Analysis {
  /** Discriminator for command analysis unions. */
  readonly kind = "compiler" as const;
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
  /** Executable path or name after wrapper removal. */
  readonly unwrappedExe: string;
  /** Command argv after wrapper removal. */
  readonly unwrappedArgv: readonly string[];
  /** Compiler phase and artifact content kind inferred from parsed options. */
  readonly compilerMode: CompilerMode;
  /** Source input paths resolved from parser facts and candidates. */
  readonly sourceFiles: readonly string[];
  /** Optional debug information for resolver decisions and diagnostics. */
  readonly debug?: CompilerResolveDebug;
  /** Effective output target selected from arguments, executable identity, driver defaults, or host fallback. */
  readonly target: EffectiveCompilerTarget;

  constructor(
    parsed: CompilerParseResult,
    resolved: CompilerResolveResult,
    command: AnalyzedData,
    unwrapped: UnwrappedCompilerCommand,
  ) {
    this.exe = command.exe;
    this.argv = command.argv;
    this.reads = resolved.reads;
    this.writes = resolved.writes;
    this.edges = resolved.edges;
    this.unwrappedExe = unwrapped.exe;
    this.unwrappedArgv = [...unwrapped.argv];
    this.compilerMode = { ...parsed.compilerMode };
    this.sourceFiles = [...resolved.sourceFiles];
    this.debug = resolved.debug;
    this.target = resolved.target;
  }
}

/** Analyzer for recognized compiler driver commands. */
export class CompilerAnalyzer extends Analyzer {
  readonly kind = "compiler" as const;

  private readonly identifier;
  private readonly resolver;

  constructor(options: CompilerAnalyzerOptions = {}) {
    super();
    this.identifier = options.identifier ?? new CompilerIdentifier();
    this.resolver = options.resolver ?? new CompilerResolver();
  }

  analyze(
    command: AnalyzedData,
  ): Result<CompilerAnalysis, CompilerAnalysisError> {
    return fromThrowable(
      () => {
        const unwrapped = unwrapCompilerCommand(command);
        const identity = this.identifier.identifyCompilerCommand(unwrapped);

        const parsed = parseCompilerCommand(unwrapped.argv, identity);
        const resolved = this.resolver.resolve(parsed, identity);
        return new CompilerAnalysis(parsed, resolved, command, unwrapped);
      },
      (error) => toCompilerAnalysisError(error, "compiler analysis failed"),
    )();
  }
}
