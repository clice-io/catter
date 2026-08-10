import * as cli from "catter/cli";
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

const parsed = cli.parse(command, [
  "-v",
  "-d",
  "2",
  "-Iinc",
  "-I",
  "generated",
  "main.cc",
  "util.cc",
]);

assertThrow(parsed.ok);
if (parsed.ok) {
  assertThrow(!parsed.helpRequested);
  assertThrow(parsed.values.verbose);
  assertThrow(parsed.values.depth === 2);
  assertThrow(parsed.values.include.length === 2);
  assertThrow(parsed.values.include[0] === "inc");
  assertThrow(parsed.values.include[1] === "generated");
  assertThrow(parsed.values.input === "main.cc");
  assertThrow(parsed.values.rest.length === 1);
  assertThrow(parsed.values.rest[0] === "util.cc");
  assertThrow(parsed.usage.includes("Usage:"));
  assertThrow(parsed.usage.includes("--depth <n>"));
  assertThrow(parsed.usage.includes("Examples:"));
}

const help = cli.parse(command, ["--help"]);
assertThrow(help.ok);
if (help.ok) {
  assertThrow(help.helpRequested);
  assertThrow(help.usage.includes("-h, --help"));
}

assertThrow(cli.run(command, ["--help"]) === undefined);

const failure = cli.parse(command, ["--unknown"]);
assertThrow(!failure.ok);
if (!failure.ok) {
  assertThrow(failure.error.includes("unknown option"));
  assertThrow(cli.formatError(failure).includes("Usage:"));
}

assertThrow(cli.run(command, ["--unknown"]) === undefined);

let parseErrorSeen = false;
try {
  cli.parseOrThrow(command, ["-d", "-1", "main.cc"]);
} catch (error) {
  assertThrow(error instanceof cli.CLIParseError);
  if (error instanceof cli.CLIParseError) {
    parseErrorSeen = true;
    assertThrow(error.format().includes(">= 0"));
  }
}
assertThrow(parseErrorSeen);
