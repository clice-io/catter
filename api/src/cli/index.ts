import { io } from "../index.js";

/**
 * Lightweight declarative command-line parser for catter QuickJS scripts.
 *
 * Callers pass `argv` arrays directly, usually from `config.scriptArgs`,
 * instead of relying on a Node.js global runtime.
 */
export type Simplify<T> = {
  [K in keyof T]: T[K];
};

/**
 * Converts a union type into an intersection type.
 */
export type UnionToIntersection<T> = (
  T extends unknown ? (value: T) => void : never
) extends (value: infer Result) => void
  ? Result
  : never;

/**
 * Merges a union of object types into a single object type.
 */
export type MergeUnion<T> = [T] extends [never]
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
 * Successful parse result returned by {@link parse}.
 */
export type ParseSuccess<
  Options extends readonly CommandOption[] = readonly CommandOption[],
  Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
> = {
  ok: true;
  argv: readonly string[];
  values: CommandValues<Options, Positionals>;
  usage: string;
  helpRequested: boolean;
};

/**
 * Failed parse result returned by {@link parse}.
 */
export type ParseFailure<
  Options extends readonly CommandOption[] = readonly CommandOption[],
  Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
> = {
  ok: false;
  argv: readonly string[];
  error: string;
  usage: string;
  helpRequested: false;
};

/**
 * Non-throwing parse result returned by {@link parse}.
 */
export type ParseResult<
  Options extends readonly CommandOption[] = readonly CommandOption[],
  Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
> = ParseSuccess<Options, Positionals> | ParseFailure<Options, Positionals>;

type NormalizedCommand = {
  readonly command: CommandDefinition;
  readonly options: readonly CommandOption[];
  readonly positionals: readonly AnyPositionalArgument[];
  readonly optionByLong: Map<string, CommandOption>;
  readonly optionByShort: Map<string, CommandOption>;
  readonly builtinHelp: boolean;
};

function isCommandOptionWithValue(
  option: CommandOption,
): option is
  | StringOption<string, unknown, boolean>
  | NumberOption<string, unknown, boolean> {
  return option.kind === "string" || option.kind === "number";
}

function cloneDefaultValue<Value>(
  value: readonly Value[] | Value,
): readonly Value[] | Value {
  return Array.isArray(value) ? [...value] : value;
}

function longOptionName(option: CommandOption): string {
  return `--${option.name}`;
}

function shortOptionName(option: CommandOption): string | undefined {
  return option.short === undefined ? undefined : `-${option.short}`;
}

function optionDisplayName(option: CommandOption): string {
  return longOptionName(option);
}

function positionalLabel(argument: AnyPositionalArgument): string {
  const label = `<${argument.valueName ?? argument.name}>`;
  if (argument.multiple) {
    return `${label}...`;
  }
  if (argument.required === false) {
    return `[${label}]`;
  }
  return label;
}

function optionValueLabel(option: CommandOption): string {
  return `<${option.valueName ?? option.name}>`;
}

function optionUsageLabel(option: CommandOption): string {
  const shortName = shortOptionName(option);
  const longName = longOptionName(option);

  if (!isCommandOptionWithValue(option)) {
    return shortName === undefined ? longName : `${shortName}, ${longName}`;
  }

  const valueLabel = optionValueLabel(option);
  const longPart = `${longName} ${valueLabel}`;
  return shortName === undefined ? longPart : `${shortName}, ${longPart}`;
}

function appendDescriptionSuffix(description: string, suffix: string): string {
  return description.length === 0 ? suffix : `${description} ${suffix}`;
}

function optionDescription(option: CommandOption): string {
  let description = option.description ?? "";

  if (option.required) {
    description = appendDescriptionSuffix(description, "(required)");
  }

  if (option.default !== undefined && option.kind !== "flag") {
    const value = Array.isArray(option.default)
      ? option.default.join(", ")
      : String(option.default);
    description = appendDescriptionSuffix(description, `[default: ${value}]`);
  }

  if (
    option.default !== undefined &&
    option.kind === "flag" &&
    option.default
  ) {
    description = appendDescriptionSuffix(description, "[default: true]");
  }

  return description;
}

function argumentDescription(argument: AnyPositionalArgument): string {
  let description = argument.description ?? "";

  if (argument.required === false) {
    description = appendDescriptionSuffix(description, "(optional)");
  }

  if (argument.default !== undefined) {
    const value = Array.isArray(argument.default)
      ? argument.default.join(", ")
      : String(argument.default);
    description = appendDescriptionSuffix(description, `[default: ${value}]`);
  }

  return description;
}

function formatColumns(rows: ReadonlyArray<readonly [string, string]>): string {
  const maxLabelLength = rows.reduce(
    (maxLength, [label]) => Math.max(maxLength, label.length),
    0,
  );

  return rows
    .map(([label, description]) =>
      description.length === 0
        ? `  ${label}`
        : `  ${label.padEnd(maxLabelLength)}  ${description}`,
    )
    .join("\n");
}

function buildUsageSynopsis(normalized: NormalizedCommand): string {
  const parts = [normalized.command.name];

  if (
    normalized.options.some((option) => !option.hidden) ||
    normalized.builtinHelp
  ) {
    parts.push("[options]");
  }

  for (const argument of normalized.positionals) {
    if (!argument.hidden) {
      parts.push(positionalLabel(argument));
    }
  }

  return parts.join(" ");
}

function normalizeCommand(command: CommandDefinition): NormalizedCommand {
  const options = command.options ?? [];
  const positionals = command.positionals ?? [];
  const optionByLong = new Map<string, CommandOption>();
  const optionByShort = new Map<string, CommandOption>();
  const valueNames = new Set<string>();
  let variadicSeen = false;

  for (const option of options) {
    if (option.name.length === 0) {
      throw new Error("cli: option name must not be empty");
    }
    if (valueNames.has(option.name)) {
      throw new Error(`cli: duplicate parsed value name: ${option.name}`);
    }
    valueNames.add(option.name);

    if (optionByLong.has(option.name)) {
      throw new Error(`cli: duplicate option name: --${option.name}`);
    }
    optionByLong.set(option.name, option);

    if (option.short !== undefined) {
      if (option.short.length !== 1) {
        throw new Error(
          `cli: short option for ${option.name} must be a single character`,
        );
      }
      if (optionByShort.has(option.short)) {
        throw new Error(`cli: duplicate short option: -${option.short}`);
      }
      optionByShort.set(option.short, option);
    }
  }

  for (let index = 0; index < positionals.length; ++index) {
    const argument = positionals[index];
    if (argument.name.length === 0) {
      throw new Error("cli: positional name must not be empty");
    }
    if (valueNames.has(argument.name)) {
      throw new Error(`cli: duplicate parsed value name: ${argument.name}`);
    }
    valueNames.add(argument.name);

    if (argument.multiple) {
      if (variadicSeen) {
        throw new Error("cli: only one variadic positional is supported");
      }
      if (index !== positionals.length - 1) {
        throw new Error("cli: variadic positional must be the last argument");
      }
      variadicSeen = true;
    }
  }

  const builtinHelp =
    command.help !== false &&
    !optionByLong.has("help") &&
    !optionByShort.has("h");

  return {
    command,
    options,
    positionals,
    optionByLong,
    optionByShort,
    builtinHelp,
  };
}

function initializeValues(
  normalized: NormalizedCommand,
): Record<string, unknown> {
  const values: Record<string, unknown> = {};

  for (const option of normalized.options) {
    if (option.kind === "flag") {
      values[option.name] = option.default ?? false;
      continue;
    }

    if (option.multiple) {
      values[option.name] =
        option.default === undefined ? [] : cloneDefaultValue(option.default);
      continue;
    }

    if (option.default !== undefined) {
      values[option.name] = cloneDefaultValue(option.default);
    }
  }

  for (const argument of normalized.positionals) {
    if (argument.multiple) {
      values[argument.name] =
        argument.default === undefined
          ? []
          : cloneDefaultValue(argument.default);
      continue;
    }

    if (argument.default !== undefined) {
      values[argument.name] = cloneDefaultValue(argument.default);
    }
  }

  return values;
}

function assignOptionValue(
  values: Record<string, unknown>,
  option: CommandOption,
  value: unknown,
): void {
  if (option.kind === "flag") {
    values[option.name] = value;
    return;
  }

  if (option.multiple) {
    const list = values[option.name];
    if (Array.isArray(list)) {
      list.push(value);
      return;
    }
    values[option.name] = [value];
    return;
  }

  values[option.name] = value;
}

function parseTypedValue(
  commandName: string,
  option: CommandOption,
  rawValue: string,
): unknown {
  if (option.kind === "flag") {
    throw new Error(
      `${commandName}: option ${optionDisplayName(option)} does not take a value`,
    );
  }

  if (option.kind === "string") {
    if (option.parse === undefined) {
      return rawValue;
    }

    try {
      return option.parse(rawValue);
    } catch (error) {
      const message =
        error instanceof Error && error.message.length > 0
          ? error.message
          : String(error);
      throw new Error(
        `${commandName}: invalid value for ${optionDisplayName(option)}: ${message}`,
      );
    }
  }

  const parsed = Number(rawValue);
  if (Number.isNaN(parsed)) {
    throw new Error(
      `${commandName}: expected a number for ${optionDisplayName(option)}, got ${rawValue}`,
    );
  }
  if (option.integer && !Number.isInteger(parsed)) {
    throw new Error(
      `${commandName}: expected an integer for ${optionDisplayName(option)}, got ${rawValue}`,
    );
  }
  if (option.min !== undefined && parsed < option.min) {
    throw new Error(
      `${commandName}: expected ${optionDisplayName(option)} >= ${option.min}, got ${rawValue}`,
    );
  }
  if (option.max !== undefined && parsed > option.max) {
    throw new Error(
      `${commandName}: expected ${optionDisplayName(option)} <= ${option.max}, got ${rawValue}`,
    );
  }

  if (option.parse === undefined) {
    return parsed;
  }

  try {
    return option.parse(parsed);
  } catch (error) {
    const message =
      error instanceof Error && error.message.length > 0
        ? error.message
        : String(error);
    throw new Error(
      `${commandName}: invalid value for ${optionDisplayName(option)}: ${message}`,
    );
  }
}

function parsePositionalValue(
  commandName: string,
  argument: AnyPositionalArgument,
  rawValue: string,
): unknown {
  if (argument.parse === undefined) {
    return rawValue;
  }

  try {
    return argument.parse(rawValue);
  } catch (error) {
    const message =
      error instanceof Error && error.message.length > 0
        ? error.message
        : String(error);
    throw new Error(
      `${commandName}: invalid value for <${argument.valueName ?? argument.name}>: ${message}`,
    );
  }
}

function finalizeValues(
  normalized: NormalizedCommand,
  values: Record<string, unknown>,
  positionals: readonly string[],
): void {
  for (const option of normalized.options) {
    if (!option.required) {
      continue;
    }

    if (option.kind === "flag") {
      if (!values[option.name]) {
        throw new Error(
          `${normalized.command.name}: missing required option ${optionDisplayName(option)}`,
        );
      }
      continue;
    }

    if (option.multiple) {
      const list = values[option.name];
      if (!Array.isArray(list) || list.length === 0) {
        throw new Error(
          `${normalized.command.name}: missing required option ${optionDisplayName(option)}`,
        );
      }
      continue;
    }

    if (values[option.name] === undefined) {
      throw new Error(
        `${normalized.command.name}: missing required option ${optionDisplayName(option)}`,
      );
    }
  }

  let position = 0;
  for (const argument of normalized.positionals) {
    if (argument.multiple) {
      const remaining = positionals
        .slice(position)
        .map((value) =>
          parsePositionalValue(normalized.command.name, argument, value),
        );
      if (argument.required !== false && remaining.length === 0) {
        throw new Error(
          `${normalized.command.name}: missing required argument <${argument.valueName ?? argument.name}>`,
        );
      }
      values[argument.name] = remaining;
      position = positionals.length;
      continue;
    }

    const value = positionals[position];
    if (value === undefined) {
      if (argument.required !== false && argument.default === undefined) {
        throw new Error(
          `${normalized.command.name}: missing required argument <${argument.valueName ?? argument.name}>`,
        );
      }
      if (argument.default === undefined) {
        values[argument.name] = undefined;
      }
      continue;
    }

    values[argument.name] = parsePositionalValue(
      normalized.command.name,
      argument,
      value,
    );
    ++position;
  }

  if (position < positionals.length) {
    throw new Error(
      `${normalized.command.name}: unexpected positional argument: ${positionals[position]}`,
    );
  }
}

type RawParseSuccess = {
  ok: true;
  argv: readonly string[];
  values: Record<string, unknown>;
  usage: string;
  helpRequested: boolean;
};

type RawParseFailure = {
  ok: false;
  argv: readonly string[];
  error: string;
  usage: string;
  helpRequested: false;
};

function parseInternal(
  normalized: NormalizedCommand,
  argv: readonly string[],
): RawParseSuccess | RawParseFailure {
  const usageText = usage(normalized.command);
  const values = initializeValues(normalized);
  const positionalValues: string[] = [];
  let optionMode = true;

  const fail = (error: string): RawParseFailure => ({
    ok: false,
    argv: [...argv],
    error,
    usage: usageText,
    helpRequested: false,
  });

  const succeed = (helpRequested = false): RawParseSuccess => ({
    ok: true,
    argv: [...argv],
    values,
    usage: usageText,
    helpRequested,
  });

  for (let index = 0; index < argv.length; ++index) {
    const arg = argv[index];

    if (
      optionMode &&
      normalized.builtinHelp &&
      (arg === "-h" || arg === "--help")
    ) {
      return succeed(true);
    }

    if (optionMode && arg === "--") {
      optionMode = false;
      continue;
    }

    if (optionMode && arg.startsWith("--") && arg.length > 2) {
      const body = arg.slice(2);
      const eqIndex = body.indexOf("=");
      const longName = eqIndex === -1 ? body : body.slice(0, eqIndex);
      const attachedValue =
        eqIndex === -1 ? undefined : body.slice(eqIndex + 1);
      const option = normalized.optionByLong.get(longName);

      if (option === undefined) {
        return fail(
          `${normalized.command.name}: unknown option: --${longName}`,
        );
      }

      if (option.kind === "flag") {
        if (attachedValue !== undefined) {
          return fail(
            `${normalized.command.name}: option ${optionDisplayName(option)} does not take a value`,
          );
        }
        assignOptionValue(values, option, true);
        continue;
      }

      const rawValue =
        attachedValue !== undefined ? attachedValue : argv[index + 1];
      if (rawValue === undefined) {
        return fail(
          `${normalized.command.name}: missing value for ${optionDisplayName(option)}`,
        );
      }

      if (attachedValue === undefined) {
        ++index;
      }

      try {
        assignOptionValue(
          values,
          option,
          parseTypedValue(normalized.command.name, option, rawValue),
        );
      } catch (error) {
        return fail(error instanceof Error ? error.message : String(error));
      }
      continue;
    }

    if (optionMode && arg.startsWith("-") && arg !== "-" && arg.length > 1) {
      for (let shortIndex = 1; shortIndex < arg.length; ++shortIndex) {
        const shortName = arg[shortIndex];
        const option = normalized.optionByShort.get(shortName);
        if (option === undefined) {
          return fail(
            `${normalized.command.name}: unknown option: -${shortName}`,
          );
        }

        if (option.kind === "flag") {
          assignOptionValue(values, option, true);
          continue;
        }

        const remainder = arg.slice(shortIndex + 1);
        const inlineValue =
          remainder.length === 0
            ? undefined
            : remainder.startsWith("=")
              ? remainder.slice(1)
              : remainder;
        const rawValue =
          inlineValue !== undefined ? inlineValue : argv[index + 1];
        if (rawValue === undefined) {
          return fail(
            `${normalized.command.name}: missing value for ${optionDisplayName(option)}`,
          );
        }

        if (inlineValue === undefined) {
          ++index;
        }

        try {
          assignOptionValue(
            values,
            option,
            parseTypedValue(normalized.command.name, option, rawValue),
          );
        } catch (error) {
          return fail(error instanceof Error ? error.message : String(error));
        }
        break;
      }
      continue;
    }

    positionalValues.push(arg);
  }

  try {
    finalizeValues(normalized, values, positionalValues);
  } catch (error) {
    return fail(error instanceof Error ? error.message : String(error));
  }

  return succeed(false);
}

/**
 * Error thrown by {@link parseOrThrow}.
 *
 * `helpRequested === true` means the caller asked for help and `usage`
 * contains the full formatted help text.
 */
export class CLIParseError extends Error {
  readonly usage: string;
  readonly helpRequested: boolean;

  constructor(message: string, usage: string, helpRequested = false) {
    super(message);
    this.name = "CLIParseError";
    this.usage = usage;
    this.helpRequested = helpRequested;
  }

  format(): string {
    if (this.helpRequested) {
      return this.usage;
    }

    return `${this.message}\n\n${this.usage}`;
  }
}

/**
 * Validates and returns a declarative command definition.
 *
 * This helper preserves literal option and positional names so the parsed
 * `values` object remains strongly typed.
 */
export function command<
  const Options extends readonly CommandOption[] = readonly CommandOption[],
  const Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
>(definition: {
  name: string;
  description?: string;
  options?: Options;
  positionals?: Positionals;
  help?: boolean;
  examples?: readonly CLIExample[];
}): CommandDefinition<Options, Positionals> {
  normalizeCommand(definition);
  return definition;
}

/**
 * Creates a boolean flag option.
 */
export function flag<const Name extends string>(
  name: Name,
  options: Omit<FlagOption<Name>, "kind" | "name"> = {},
): FlagOption<Name> {
  return {
    kind: "flag",
    name,
    ...options,
  };
}

/**
 * Creates a string option.
 */
export function string<const Name extends string>(
  name: Name,
  options?: Omit<
    StringOption<Name, string, false>,
    "kind" | "name" | "parse"
  > & {
    parse?: undefined;
    multiple?: false;
  },
): StringOption<Name, string, false>;
export function string<const Name extends string>(
  name: Name,
  options: Omit<
    StringOption<Name, string, true>,
    "kind" | "name" | "parse" | "multiple"
  > & {
    parse?: undefined;
    multiple: true;
  },
): StringOption<Name, string, true>;
export function string<const Name extends string, Value>(
  name: Name,
  options: Omit<
    StringOption<Name, Value, false>,
    "kind" | "name" | "multiple"
  > & {
    multiple?: false;
  },
): StringOption<Name, Value, false>;
export function string<const Name extends string, Value>(
  name: Name,
  options: Omit<
    StringOption<Name, Value, true>,
    "kind" | "name" | "multiple"
  > & {
    multiple: true;
  },
): StringOption<Name, Value, true>;
export function string(
  name: string,
  options: Omit<StringOption<string, unknown, boolean>, "kind" | "name"> = {},
): StringOption<string, unknown, boolean> {
  return {
    kind: "string",
    name,
    ...options,
  };
}

/**
 * Creates a number option.
 */
export function number<const Name extends string>(
  name: Name,
  options?: Omit<
    NumberOption<Name, number, false>,
    "kind" | "name" | "parse"
  > & {
    parse?: undefined;
    multiple?: false;
  },
): NumberOption<Name, number, false>;
export function number<const Name extends string>(
  name: Name,
  options: Omit<
    NumberOption<Name, number, true>,
    "kind" | "name" | "parse" | "multiple"
  > & {
    parse?: undefined;
    multiple: true;
  },
): NumberOption<Name, number, true>;
export function number<const Name extends string, Value>(
  name: Name,
  options: Omit<
    NumberOption<Name, Value, false>,
    "kind" | "name" | "multiple"
  > & {
    multiple?: false;
  },
): NumberOption<Name, Value, false>;
export function number<const Name extends string, Value>(
  name: Name,
  options: Omit<
    NumberOption<Name, Value, true>,
    "kind" | "name" | "multiple"
  > & {
    multiple: true;
  },
): NumberOption<Name, Value, true>;
export function number(
  name: string,
  options: Omit<NumberOption<string, unknown, boolean>, "kind" | "name"> = {},
): NumberOption<string, unknown, boolean> {
  return {
    kind: "number",
    name,
    ...options,
  };
}

/**
 * Creates a positional argument.
 */
export function positional<const Name extends string>(
  name: Name,
  options?: Omit<
    PositionalArgument<Name, string, false>,
    "kind" | "name" | "parse"
  > & {
    parse?: undefined;
    multiple?: false;
  },
): PositionalArgument<Name, string, false>;
export function positional<const Name extends string>(
  name: Name,
  options: Omit<
    PositionalArgument<Name, string, true>,
    "kind" | "name" | "parse" | "multiple"
  > & {
    parse?: undefined;
    multiple: true;
  },
): PositionalArgument<Name, string, true>;
export function positional<const Name extends string, Value>(
  name: Name,
  options: Omit<
    PositionalArgument<Name, Value, false>,
    "kind" | "name" | "multiple"
  > & {
    multiple?: false;
  },
): PositionalArgument<Name, Value, false>;
export function positional<const Name extends string, Value>(
  name: Name,
  options: Omit<
    PositionalArgument<Name, Value, true>,
    "kind" | "name" | "multiple"
  > & {
    multiple: true;
  },
): PositionalArgument<Name, Value, true>;
export function positional(
  name: string,
  options: Omit<
    PositionalArgument<string, unknown, boolean>,
    "kind" | "name"
  > = {},
): PositionalArgument<string, unknown, boolean> {
  return {
    kind: "positional",
    name,
    ...options,
  };
}

/**
 * Renders human-readable help text for a command definition.
 */
export function usage(command: CommandDefinition): string {
  const normalized = normalizeCommand(command);
  const sections: string[] = [];

  if (command.description !== undefined && command.description.length > 0) {
    sections.push(command.description);
  }

  sections.push(`Usage:\n  ${buildUsageSynopsis(normalized)}`);

  const visiblePositionals = normalized.positionals.filter(
    (argument) => !argument.hidden,
  );
  if (visiblePositionals.length > 0) {
    sections.push(
      `Arguments:\n${formatColumns(
        visiblePositionals.map((argument) => [
          positionalLabel(argument),
          argumentDescription(argument),
        ]),
      )}`,
    );
  }

  const optionRows = normalized.options
    .filter((option) => !option.hidden)
    .map(
      (option) =>
        [optionUsageLabel(option), optionDescription(option)] as const,
    );
  if (normalized.builtinHelp) {
    optionRows.push(["-h, --help", "Print this help message."]);
  }
  if (optionRows.length > 0) {
    sections.push(`Options:\n${formatColumns(optionRows)}`);
  }

  if (command.examples !== undefined && command.examples.length > 0) {
    const exampleLines = command.examples.map((example) => {
      if (typeof example === "string") {
        return `  ${example}`;
      }

      if (
        example.description === undefined ||
        example.description.length === 0
      ) {
        return `  ${example.command}`;
      }

      return `  ${example.command}\n  # ${example.description}`;
    });
    sections.push(`Examples:\n${exampleLines.join("\n")}`);
  }

  return sections.join("\n\n");
}

/**
 * Parses an argv array against a command definition without throwing.
 */
export function parse<
  const Options extends readonly CommandOption[] = readonly CommandOption[],
  const Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
>(
  command: CommandDefinition<Options, Positionals>,
  argv: readonly string[],
): ParseResult<Options, Positionals> {
  return parseInternal(normalizeCommand(command), argv) as ParseResult<
    Options,
    Positionals
  >;
}

/**
 * Parses an argv array and throws {@link CLIParseError} on failure or help.
 */
export function parseOrThrow<
  const Options extends readonly CommandOption[] = readonly CommandOption[],
  const Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
>(
  command: CommandDefinition<Options, Positionals>,
  argv: readonly string[],
): ParseSuccess<Options, Positionals> {
  const result = parse(command, argv);
  if (!result.ok) {
    throw new CLIParseError(result.error, result.usage);
  }
  if (result.helpRequested) {
    throw new CLIParseError(result.usage, result.usage, true);
  }
  return result as ParseSuccess<Options, Positionals>;
}

/**
 * Formats a parse error into a human-readable message followed by usage text.
 */
export function formatError(value: ParseFailure | CLIParseError): string {
  if (value instanceof CLIParseError) {
    return value.format();
  }

  return `${value.error}\n\n${value.usage}`;
}

/**
 * Convenience helper that parses argv, prints any help or error text to stdout,
 * and returns parsed values on success.
 */
export function run<
  const Options extends readonly CommandOption[] = readonly CommandOption[],
  const Positionals extends
    readonly AnyPositionalArgument[] = readonly AnyPositionalArgument[],
>(
  command: CommandDefinition<Options, Positionals>,
  argv: readonly string[],
): CommandValues<Options, Positionals> | undefined {
  const res = parse(command, argv);
  if (!res.ok) {
    io.println(formatError(res));
    return undefined;
  }
  if (res.helpRequested) {
    io.println(res.usage);
    return undefined;
  }
  return res.values;
}
