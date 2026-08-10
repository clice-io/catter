import type { Result } from "catter/neverthrow";

/**
 * Shared type-level helpers used to infer parsed value shapes.
 */
type Simplify<T> = {
  [K in keyof T]: T[K];
};

/**
 * Converts a union type into an intersection type.
 */
type UnionToIntersection<T> = (
  T extends unknown ? (value: T) => void : never
) extends (value: infer Result) => void
  ? Result
  : never;

/**
 * Merges a union of object types into a single object type.
 */
type MergeUnion<T> = [T] extends [never]
  ? object
  : Simplify<UnionToIntersection<T>>;

/**
 * Example entry rendered in generated usage output.
 */
export type CLIExample =
  | string
  | {
      command: string;
      description?: string;
    };

/**
 * Shared metadata for named parsed values.
 */
export type BaseNamedValue<Name extends string> = {
  /**
   * Programmatic key used in the parsed result.
   */
  name: Name;

  /**
   * Human-readable explanation used in generated usage text.
   */
  description?: string;

  /**
   * Omits this item from the rendered usage text.
   */
  hidden?: boolean;
};

/**
 * Shared metadata for named options.
 */
export type BaseOption<
  Name extends string,
  Multiple extends boolean,
> = BaseNamedValue<Name> & {
  /**
   * Single-character short option name without the leading `-`.
   */
  short?: string;

  /**
   * Whether the option must be provided by the caller.
   */
  required?: boolean;

  /**
   * Allows the option to be repeated and collected into an array.
   */
  multiple?: Multiple;

  /**
   * Placeholder label used in rendered usage output.
   */
  valueName?: string;
};

/**
 * Boolean option definition such as `-v` or `--verbose`.
 */
export type FlagOption<Name extends string = string> = Readonly<
  BaseOption<Name, false> & {
    kind: "flag";
    default?: boolean;
  }
>;

/**
 * String-valued option definition.
 */
export type StringOption<
  Name extends string = string,
  Value = string,
  Multiple extends boolean = false,
> = Readonly<
  BaseOption<Name, Multiple> & {
    kind: "string";
    default?: Multiple extends true ? readonly Value[] : Value;
    parse?: (value: string) => Value;
  }
>;

/**
 * Number-valued option definition.
 */
export type NumberOption<
  Name extends string = string,
  Value = number,
  Multiple extends boolean = false,
> = Readonly<
  BaseOption<Name, Multiple> & {
    kind: "number";
    default?: Multiple extends true ? readonly Value[] : Value;
    integer?: boolean;
    min?: number;
    max?: number;
    parse?: (value: number) => Value;
  }
>;

export type CommandOption =
  | FlagOption<string>
  | StringOption<string, unknown, boolean>
  | NumberOption<string, unknown, boolean>;

/**
 * Positional argument definition.
 */
export type PositionalArgument<
  Name extends string = string,
  Value = string,
  Multiple extends boolean = false,
> = Readonly<
  BaseNamedValue<Name> & {
    kind: "positional";
    required?: boolean;
    multiple?: Multiple;
    valueName?: string;
    default?: Multiple extends true ? readonly Value[] : Value;
    parse?: (value: string) => Value;
  }
>;

/**
 * Convenience alias for any positional argument shape supported by the parser.
 */
export type AnyPositionalArgument = PositionalArgument<
  string,
  unknown,
  boolean
>;

/**
 * Declarative command specification consumed by the parser.
 */
export type CommandDefinition<
  Options extends readonly CommandOption[] = readonly CommandOption[],
  Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
> = Readonly<{
  /**
   * Command name shown in usage output.
   */
  name: string;

  /**
   * Optional one-line summary printed above the usage section.
   */
  description?: string;

  /**
   * Declared options accepted by the command.
   */
  options?: Options;

  /**
   * Declared positional arguments accepted by the command.
   */
  positionals?: Positionals;

  /**
   * Appends a built-in `-h, --help` entry to the usage text and makes
   * `parse()` treat either form as `helpRequested`.
   *
   * Defaults to `true`.
   */
  help?: boolean;

  /**
   * Optional usage examples shown at the bottom of the help text.
   */
  examples?: readonly CLIExample[];
}>;

/**
 * Type-level mapping from an option definition to its parsed runtime value.
 */
export type OptionValue<T extends CommandOption> =
  T extends FlagOption<string>
    ? boolean
    : T extends StringOption<string, infer Value, infer Multiple>
      ? Multiple extends true
        ? Value[]
        : Value | undefined
      : T extends NumberOption<string, infer Value, infer Multiple>
        ? Multiple extends true
          ? Value[]
          : Value | undefined
        : never;

/**
 * Type-level mapping from a positional definition to its parsed runtime value.
 */
export type PositionalValue<T extends AnyPositionalArgument> =
  T extends PositionalArgument<string, infer Value, infer Multiple>
    ? Multiple extends true
      ? Value[]
      : Value | undefined
    : never;

/**
 * Object entry produced from a single option definition.
 */
export type OptionEntry<T extends CommandOption> = T extends CommandOption
  ? {
      [K in T["name"]]: OptionValue<T>;
    }
  : never;

/**
 * Object entry produced from a single positional definition.
 */
export type PositionalEntry<T extends AnyPositionalArgument> =
  T extends AnyPositionalArgument
    ? {
        [K in T["name"]]: PositionalValue<T>;
      }
    : never;

/**
 * Parsed value object inferred from a command definition.
 */
export type CommandValues<
  Options extends readonly CommandOption[] = readonly CommandOption[],
  Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
> = Simplify<
  MergeUnion<OptionEntry<Options[number]>> &
    MergeUnion<PositionalEntry<Positionals[number]>>
>;

/**
 * Extracts the parsed value shape from a command definition.
 */
export type InferValues<T> =
  T extends CommandDefinition<infer Options, infer Positionals>
    ? CommandValues<Options, Positionals>
    : never;

/**
 * Successful parse payload carried by the `Ok` variant of `ParseResult`.
 */
export type ParseSuccess<
  Options extends readonly CommandOption[] = readonly CommandOption[],
  Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
> = {
  argv: readonly string[];
  values: CommandValues<Options, Positionals>;
  usage: string;
  helpRequested: boolean;
};

/**
 * Failed parse payload carried by the `Err` variant of `ParseResult`.
 */
export type ParseFailure = {
  argv: readonly string[];
  error: string;
  usage: string;
};

/**
 * Non-throwing parse result returned by `parse`.
 */
export type ParseResult<
  Options extends readonly CommandOption[] = readonly CommandOption[],
  Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
> = Result<ParseSuccess<Options, Positionals>, ParseFailure>;

/**
 * Validated command shape shared by the builder, usage renderer, and parser.
 *
 * @internal
 */
export type NormalizedCommand = {
  readonly command: CommandDefinition;
  readonly options: readonly CommandOption[];
  readonly positionals: readonly AnyPositionalArgument[];
  readonly optionByLong: Map<string, CommandOption>;
  readonly optionByShort: Map<string, CommandOption>;
  readonly builtinHelp: boolean;
};
