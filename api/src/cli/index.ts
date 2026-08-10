/**
 * Lightweight declarative command-line parser for catter scripts.
 *
 * Command definitions are constructed through the `cli` namespace; parsing,
 * help rendering, and error handling are top-level exports.
 *
 * @example
 * ```typescript
 * import { cli, run } from "catter/cli";
 *
 * const cmd = cli.command({
 *   name: "demo",
 *   options: [cli.string("output", { short: "o" })],
 * } as const);
 *
 * run(cmd, ["--output", "build/compile_commands.json"]);
 * ```
 */
export * as cli from "./builder.js";
export { CLIParseError, formatError } from "./error.js";
export { parse, parseOrThrow, run } from "./parser.js";
export { usage } from "./usage.js";
export type {
  AnyPositionalArgument,
  BaseNamedValue,
  BaseOption,
  CLIExample,
  CommandDefinition,
  CommandOption,
  CommandValues,
  FlagOption,
  InferValues,
  NumberOption,
  OptionEntry,
  OptionValue,
  ParseFailure,
  ParseResult,
  ParseSuccess,
  PositionalArgument,
  PositionalEntry,
  PositionalValue,
  StringOption,
} from "./types.js";
