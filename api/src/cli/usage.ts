import {
  isCommandOptionWithValue,
  longOptionName,
  normalizeCommand,
  shortOptionName,
} from "./normalize.js";
import type {
  AnyPositionalArgument,
  CommandDefinition,
  CommandOption,
  NormalizedCommand,
} from "./types.js";

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
