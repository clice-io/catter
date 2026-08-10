import type { ParseFailure } from "./types.js";

/**
 * Error thrown by `parseOrThrow`.
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
 * Formats a parse error into a human-readable message followed by usage text.
 */
export function formatError(value: ParseFailure | CLIParseError): string {
  if (value instanceof CLIParseError) {
    return value.format();
  }

  return `${value.error}\n\n${value.usage}`;
}
