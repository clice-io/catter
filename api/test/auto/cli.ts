import { cli, debug } from "catter";

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

debug.assertThrow(parsed.ok);
if (parsed.ok) {
  debug.assertThrow(!parsed.helpRequested);
  debug.assertThrow(parsed.values.verbose);
  debug.assertThrow(parsed.values.depth === 2);
  debug.assertThrow(parsed.values.include.length === 2);
  debug.assertThrow(parsed.values.include[0] === "inc");
  debug.assertThrow(parsed.values.include[1] === "generated");
  debug.assertThrow(parsed.values.input === "main.cc");
  debug.assertThrow(parsed.values.rest.length === 1);
  debug.assertThrow(parsed.values.rest[0] === "util.cc");
  debug.assertThrow(parsed.usage.includes("Usage:"));
  debug.assertThrow(parsed.usage.includes("--depth <n>"));
  debug.assertThrow(parsed.usage.includes("Examples:"));
}

const help = cli.parse(command, ["--help"]);
debug.assertThrow(help.ok);
if (help.ok) {
  debug.assertThrow(help.helpRequested);
  debug.assertThrow(help.usage.includes("-h, --help"));
}

debug.assertThrow(cli.run(command, ["--help"]) === undefined);

const failure = cli.parse(command, ["--unknown"]);
debug.assertThrow(!failure.ok);
if (!failure.ok) {
  debug.assertThrow(failure.error.includes("unknown option"));
  debug.assertThrow(cli.formatError(failure).includes("Usage:"));
}

debug.assertThrow(cli.run(command, ["--unknown"]) === undefined);

let parseErrorSeen = false;
try {
  cli.parseOrThrow(command, ["-d", "-1", "main.cc"]);
} catch (error) {
  debug.assertThrow(error instanceof cli.CLIParseError);
  if (error instanceof cli.CLIParseError) {
    parseErrorSeen = true;
    debug.assertThrow(error.format().includes(">= 0"));
  }
}
debug.assertThrow(parseErrorSeen);
