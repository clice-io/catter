import {
  CLIParseError,
  cli,
  formatError,
  parse,
  parseOrThrow,
  run,
} from "catter/cli";
import { assertThrow } from "catter/debug";

const commandOptions = [
  cli.flag("verbose", {
    short: "v",
    description: "Enable verbose output.",
  }),
  cli.number("depth", {
    short: "d",
    valueName: "n",
    description: "Traversal depth.",
    integer: true,
    min: 0,
  }),
  cli.string("include", {
    short: "I",
    valueName: "path",
    description: "Additional include path.",
    multiple: true,
  }),
] as const;

const commandPositionals = [
  cli.positional("input", {
    description: "Primary input file.",
  }),
  cli.positional("rest", {
    description: "Additional files.",
    multiple: true,
    required: false,
  }),
] as const;

const command = cli.command({
  name: "demo",
  description: "Demo command for cli parser coverage.",
  options: commandOptions,
  positionals: commandPositionals,
  examples: [
    {
      command: "demo -d 2 main.cc util.cc",
      description: "Parse options and positional arguments together.",
    },
  ] as const,
});

const parsed = parse(command, [
  "-v",
  "-d",
  "2",
  "-Iinc",
  "-I",
  "generated",
  "main.cc",
  "util.cc",
]);

assertThrow(parsed.isOk());
if (parsed.isOk()) {
  assertThrow(!parsed.value.helpRequested);
  assertThrow(parsed.value.values.verbose);
  assertThrow(parsed.value.values.depth === 2);
  assertThrow(parsed.value.values.include.length === 2);
  assertThrow(parsed.value.values.include[0] === "inc");
  assertThrow(parsed.value.values.include[1] === "generated");
  assertThrow(parsed.value.values.input === "main.cc");
  assertThrow(parsed.value.values.rest.length === 1);
  assertThrow(parsed.value.values.rest[0] === "util.cc");
  assertThrow(parsed.value.usage.includes("Usage:"));
  assertThrow(parsed.value.usage.includes("--depth <n>"));
  assertThrow(parsed.value.usage.includes("Examples:"));
}

const help = parse(command, ["--help"]);
assertThrow(help.isOk());
if (help.isOk()) {
  assertThrow(help.value.helpRequested);
  assertThrow(help.value.usage.includes("-h, --help"));
}

assertThrow(run(command, ["--help"]) === undefined);

const failure = parse(command, ["--unknown"]);
assertThrow(failure.isErr());
if (failure.isErr()) {
  assertThrow(failure.error.error.includes("unknown option"));
  assertThrow(formatError(failure.error).includes("Usage:"));
}

assertThrow(run(command, ["--unknown"]) === undefined);

let parseErrorSeen = false;
try {
  parseOrThrow(command, ["-d", "-1", "main.cc"]);
} catch (error) {
  assertThrow(error instanceof CLIParseError);
  if (error instanceof CLIParseError) {
    parseErrorSeen = true;
    assertThrow(error.format().includes(">= 0"));
  }
}
assertThrow(parseErrorSeen);
