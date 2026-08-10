import { normalizeCommand } from "./normalize.js";
import type {
  AnyPositionalArgument,
  CLIExample,
  CommandDefinition,
  CommandOption,
  FlagOption,
  NumberOption,
  PositionalArgument,
  StringOption,
} from "./types.js";

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
