import type { Analysis, Analyzer } from "./model.js";

/**
 * Ordered registry of command analyzers.
 *
 * The registry asks each analyzer in registration order whether it supports a
 * command, then returns the first successful analysis.
 *
 * @example
 * ```ts
 * const registry = new cmd.Registry().register(cmd.CompilerAnalysis);
 * const analysis = registry.analyze(["clang", "-c", "main.c"]);
 * ```
 */
export class Registry {
  private readonly analyzerList: Analyzer[] = [];

  /**
   * Registers an analyzer class or object under its `key`.
   *
   * If another analyzer with the same key already exists, it is replaced.
   *
   * @example
   * ```ts
   * const registry = new cmd.Registry();
   * registry.register(cmd.CompilerAnalysis);
   * ```
   */
  register<A extends Analysis>(analyzer: Analyzer<A>): this {
    this.unregister(analyzer.key);
    this.analyzerList.push(analyzer as Analyzer);
    return this;
  }

  /**
   * Removes a previously registered analyzer by key.
   *
   * @example
   * ```ts
   * const registry = new cmd.Registry().register(cmd.CompilerAnalysis);
   * registry.unregister(cmd.CompilerAnalysis.key);
   * ```
   */
  unregister(key: string): this {
    const index = this.analyzerList.findIndex((item) => item.key === key);
    if (index !== -1) {
      this.analyzerList.splice(index, 1);
    }
    return this;
  }

  /**
   * Returns the currently registered analyzers in match order.
   *
   * @example
   * ```ts
   * const analyzers = new cmd.Registry()
   *   .register(cmd.CompilerAnalysis)
   *   .analyzers();
   * ```
   */
  analyzers(): readonly Analyzer[] {
    return this.analyzerList;
  }

  /**
   * Checks whether any registered analyzer claims the command.
   *
   * @example
   * ```ts
   * const ok = new cmd.Registry()
   *   .register(cmd.CompilerAnalysis)
   *   .canHandle(["gcc", "-c", "main.c"]);
   * ```
   */
  canHandle(cmd: readonly string[]): boolean {
    return this.analyzerList.some((analyzer) => analyzer.supports(cmd));
  }

  /**
   * Runs the first matching analyzer and returns its result.
   *
   * @example
   * ```ts
   * const analysis = new cmd.Registry()
   *   .register(cmd.CompilerAnalysis)
   *   .analyze(["clang", "-c", "main.c"]);
   * ```
   */
  analyze(cmd: readonly string[]): Analysis | undefined {
    for (const analyzer of this.analyzerList) {
      if (!analyzer.supports(cmd)) {
        continue;
      }

      const result = analyzer.analyze(cmd);
      if (result !== undefined) {
        return result;
      }
    }

    return undefined;
  }
}
