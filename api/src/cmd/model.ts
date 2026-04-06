/**
 * Describes one concrete dependency edge produced by a command.
 *
 * @example
 * ```ts
 * const edge: cmd.Edge = {
 *   output: "app.o",
 *   inputs: ["app.c"],
 * };
 * ```
 */
export interface Edge {
  output: string;
  inputs: string[];
}

/**
 * Base class for one analyzed command invocation.
 *
 * `consume` records the files the command reads, `produce` records the files it
 * writes, and `edges()` can refine that into explicit
 * output-to-input mappings when a subclass knows more.
 *
 * @example
 * ```ts
 * class ToyAnalysis extends cmd.Analysis<"toy"> {
 *   constructor() {
 *     super("toy", ["in.dat"], ["out.pkg"]);
 *   }
 * }
 * ```
 */
export abstract class Analysis<Exe extends string = string> {
  /** Normalized executable identifier for the analyzed command. */
  readonly exe: Exe;
  /** Files consumed by the command. */
  readonly consume: string[];
  /** Files produced by the command. */
  readonly produce: string[];

  protected constructor(
    exe: Exe,
    consume: readonly string[],
    produce: readonly string[],
  ) {
    this.exe = exe;
    this.consume = [...consume];
    this.produce = [...produce];
  }

  /**
   * Returns explicit output-to-input dependency edges for this analysis.
   *
   * The default implementation connects every produced file to the full
   * `consume` list. Subclasses can override it with more precise pairings.
   *
   * @example
   * ```ts
   * const analysis = cmd.CompilerAnalysis.analyze(["clang", "-c", "main.c"]);
   * const edges = analysis?.edges();
   * ```
   */
  edges(): Edge[] {
    return this.produce.map((output) => ({
      output,
      inputs: [...this.consume],
    }));
  }
}

/**
 * Pluggable analyzer contract used by `Registry`.
 *
 * A concrete analysis type usually implements this interface through static
 * `key`, `supports()` and `analyze()` members.
 *
 * @example
 * ```ts
 * class ToyAnalysis extends cmd.Analysis<"toy"> {
 *   static readonly key = "toy";
 *   static supports(argv: readonly string[]) {
 *     return argv[0] === "toy";
 *   }
 *   static analyze(argv: readonly string[]) {
 *     return ToyAnalysis.supports(argv) ? new ToyAnalysis() : undefined;
 *   }
 *   constructor() {
 *     super("toy", [], []);
 *   }
 * }
 * ```
 */
export interface Analyzer<A extends Analysis = Analysis> {
  /** Stable registry key used for replacement and removal. */
  readonly key: string;
  /** Returns whether this analyzer recognizes the given argv. */
  supports(cmd: readonly string[]): boolean;
  /** Performs analysis and returns a typed result when successful. */
  analyze(cmd: readonly string[]): A | undefined;
}
