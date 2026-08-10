import { println } from "catter/io";
import { err, ok, type Result } from "catter/neverthrow";
import { CLIParseError, formatError } from "./error.js";
import {
  cloneDefaultValue,
  normalizeCommand,
  optionDisplayName,
} from "./normalize.js";
import type {
  AnyPositionalArgument,
  CommandDefinition,
  CommandOption,
  CommandValues,
  NormalizedCommand,
  ParseResult,
  ParseSuccess,
} from "./types.js";
import { usage } from "./usage.js";

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

  if (rawValue.trim().length === 0) {
    throw new Error(
      `${commandName}: missing value for ${optionDisplayName(option)}`,
    );
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
      if (
        argument.required !== false &&
        remaining.length === 0 &&
        argument.default === undefined
      ) {
        throw new Error(
          `${normalized.command.name}: missing required argument <${argument.valueName ?? argument.name}>`,
        );
      }
      if (remaining.length > 0 || argument.default === undefined) {
        values[argument.name] = remaining;
      }
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
  argv: readonly string[];
  values: Record<string, unknown>;
  usage: string;
  helpRequested: boolean;
};

type RawParseFailure = {
  argv: readonly string[];
  error: string;
  usage: string;
};

function parseInternal(
  normalized: NormalizedCommand,
  argv: readonly string[],
): Result<RawParseSuccess, RawParseFailure> {
  const usageText = usage(normalized.command);
  const values = initializeValues(normalized);
  const positionalValues: string[] = [];
  let optionMode = true;

  const fail = (error: string) =>
    err({ argv: [...argv], error, usage: usageText });

  const succeed = (helpRequested = false) =>
    ok({ argv: [...argv], values, usage: usageText, helpRequested });

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
 * Parses an argv array and throws `CLIParseError` on failure or help.
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
  if (result.isErr()) {
    throw new CLIParseError(result.error.error, result.error.usage);
  }
  if (result.value.helpRequested) {
    throw new CLIParseError(result.value.usage, result.value.usage, true);
  }
  return result.value;
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
  if (res.isErr()) {
    println(formatError(res.error));
    return undefined;
  }
  if (res.value.helpRequested) {
    println(res.value.usage);
    return undefined;
  }
  return res.value.values;
}
