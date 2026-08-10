import type {
  AnyPositionalArgument,
  CommandDefinition,
  CommandOption,
  NormalizedCommand,
  NumberOption,
  StringOption,
} from "./types.js";

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

/**
 * Validates a command definition and builds the lookup tables used by the
 * usage renderer and the parser.
 */
export function normalizeCommand(command: CommandDefinition): NormalizedCommand {
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

export {
  cloneDefaultValue,
  isCommandOptionWithValue,
  longOptionName,
  optionDisplayName,
  shortOptionName,
};
