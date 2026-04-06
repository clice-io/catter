import type { Analysis, Analyzer } from "./model.js";
import { ArchiverAnalysis } from "./archiver-cmd.js";
import { CompilerAnalysis } from "./compiler-cmd.js";
import { Registry } from "./registry.js";

/**
 * Shared registry populated with the built-in analyzers.
 *
 * At the moment this includes compiler-like drivers and archivers.
 *
 * @example
 * ```ts
 * const analysis = cmd.defaultRegistry.analyze(["clang", "-c", "main.c"]);
 * ```
 */
export const defaultRegistry = new Registry()
  .register(CompilerAnalysis)
  .register(ArchiverAnalysis);

/**
 * Analyzes a command with the built-in registry.
 *
 * @example
 * ```ts
 * const analysis = cmd.analyze(["llvm-ar", "rcs", "liba.a", "a.o"]);
 * ```
 */
export function analyze(cmd: readonly string[]): Analysis | undefined {
  return defaultRegistry.analyze(cmd);
}

/**
 * Checks whether the built-in registry recognizes a command.
 *
 * @example
 * ```ts
 * const ok = cmd.canHandle(["clang", "-c", "main.c"]);
 * ```
 */
export function canHandle(cmd: readonly string[]): boolean {
  return defaultRegistry.canHandle(cmd);
}

/**
 * Registers an analyzer into the shared built-in registry.
 *
 * @example
 * ```ts
 * cmd.register(cmd.CompilerAnalysis);
 * ```
 */
export function register<A extends Analysis>(analyzer: Analyzer<A>): Registry {
  return defaultRegistry.register(analyzer);
}

/**
 * Unregisters an analyzer from the shared built-in registry.
 *
 * @example
 * ```ts
 * cmd.unregister(cmd.CompilerAnalysis.key);
 * ```
 */
export function unregister(key: string): Registry {
  return defaultRegistry.unregister(key);
}
