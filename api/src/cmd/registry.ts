import { err, ok, type Result } from "catter/neverthrow";
import {
  AnalysisError,
  type Analysis,
  type AnalyzedData,
  type IAnalyzer,
} from "./model.js";

/**
 * Ordered registry of command analyzers.
 *
 * The registry asks each analyzer in registration order to analyze a command,
 * then returns the first successful analysis.
 *
 * @example
 * ```ts
 * const registry = new cmd.Registry().register("compiler", new cmd.CompilerAnalyzer());
 * const analysis = registry.analyze({ exe: "clang", argv: ["clang", "-c", "main.c"] });
 * ```
 */
export class Registry<
  T extends Analysis = Analysis,
  R extends AnalysisError = AnalysisError,
> {
  private readonly analyzerMap = new Map<string, IAnalyzer<T, R>>();

  /**
   * Registers an analyzer instance.
   *
   * @example
   * ```ts
   * const registry = new cmd.Registry();
   * registry.register("compiler", new cmd.CompilerAnalyzer());
   * ```
   */
  register(key: string, analyzer: IAnalyzer<T, R>): this {
    this.analyzerMap.delete(key);
    this.analyzerMap.set(key, analyzer);
    return this;
  }

  /**
   * Removes a previously registered analyzer by key.
   *
   * @example
   * ```ts
   * const analyzer = new cmd.CompilerAnalyzer();
   * const registry = new cmd.Registry().register("compiler", analyzer);
   * registry.unregister("compiler");
   * ```
   */
  unregister(key: string): this {
    this.analyzerMap.delete(key);
    return this;
  }

  /**
   * Returns the currently registered analyzers in match order.
   *
   * @example
   * ```ts
   * const analyzers = new cmd.Registry()
   *   .register("compiler", new cmd.CompilerAnalyzer())
   *   .analyzers();
   * ```
   */
  analyzers(): readonly IAnalyzer<T, R>[] {
    return [...this.analyzerMap.values()];
  }

  /** Runs analyzers in order and returns the first successful analysis. */
  analyze(command: AnalyzedData): Result<T, R[]> {
    const errors: R[] = [];

    for (const analyzer of this.analyzerMap.values()) {
      const result = analyzer.analyze(command);
      if (result.isOk()) {
        return ok(result.value);
      }
      errors.push(result.error);
    }

    return err(errors);
  }
}
