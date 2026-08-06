import * as data from "catter/data";
import * as service from "catter/service";

import * as cli from "catter/cli";
import * as io from "catter/io";
import * as fs from "catter/fs";
import {
  CompilerAnalyzer,
  CompilerResolver,
  type CompilerAnalysis,
  type CompilerAnalysisError,
  type CompilerResolveDebug,
} from "catter/cmd";
import {
  CDBManager,
  type CDBCommand,
  type CDBEntry,
  type CDBItem,
  cdbItemsOf,
} from "catter/cdb";

type Producer = CDBCommand;

type CDBScriptOptions = {
  outputPath: string;
  append: boolean;
  saveOnFailure: boolean;
  abortOnCommandFailure: boolean;
  abortOnCaptureError: boolean;
  quiet: boolean;
  verbose: boolean;
};

const cdbCLI = cli.command({
  name: "cdb",
  description:
    "Generate a compile_commands.json file from captured compiler commands.",
  options: [
    cli.string("output", {
      short: "o",
      valueName: "path",
      description: "Write the compilation database to this path.",
    }),
    cli.flag("append", {
      description: "Merge with an existing database. This is the default.",
    }),
    cli.flag("replace", {
      description:
        "Ignore existing database entries and replace the output file.",
    }),
    cli.flag("save-on-failure", {
      description:
        "Save collected entries even when the build exits with a non-zero code.",
    }),
    cli.flag("abort-on-command-failure", {
      description:
        "Abort when any captured command exits with a non-zero code.",
    }),
    cli.flag("abort-on-capture-error", {
      description: "Abort when catter reports a command capture error.",
    }),
    cli.flag("quiet", {
      short: "q",
      description: "Suppress informational output.",
    }),
    cli.flag("verbose", {
      short: "v",
      description: "Print detailed command analysis and generated entries.",
    }),
  ] as const,
  positionals: [
    cli.positional("path", {
      required: false,
      valueName: "path",
      description: "Legacy output path; prefer --output for scripts.",
    }),
  ] as const,
  examples: [
    "cdb -o build/compile_commands.json",
    {
      command: "cdb --save-on-failure -o compile_commands.json",
      description:
        "Merge existing entries and still save partial results from a failed build.",
    },
  ],
});

function isSet<T>(value: T | undefined): value is T {
  return value !== undefined;
}

function pathOf(cwd: string, path: string): string | undefined {
  if (path === "-") {
    return undefined;
  }

  const base = fs.path.absolute(cwd);
  const joined = fs.path.isAbsolute(path) ? path : fs.path.joinAll(base, path);
  return fs.path.lexicalNormal(joined);
}

function defaultOptions(outputPath: string): CDBScriptOptions {
  return {
    outputPath,
    append: true,
    saveOnFailure: false,
    abortOnCommandFailure: false,
    abortOnCaptureError: false,
    quiet: false,
    verbose: false,
  };
}

function log(options: CDBScriptOptions, message: string): void {
  if (!options.quiet) {
    io.println(message);
  }
}

function verboseLog(options: CDBScriptOptions, message: string): void {
  if (options.verbose && !options.quiet) {
    io.println(message);
  }
}

function quoteArgument(argument: string): string {
  return /^[A-Za-z0-9_@%+=:,./\\-]+$/.test(argument)
    ? argument
    : JSON.stringify(argument);
}

function commandLine(command: service.CommandData): string {
  const argv = command.argv.length === 0 ? [command.exe] : command.argv;
  return argv.map(quoteArgument).join(" ");
}

function indentedList(label: string, values: readonly string[]): string[] {
  if (values.length === 0) {
    return [`  ${label}: none recognized`];
  }
  return [`  ${label}:`, ...values.map((value) => `    ${value}`)];
}

function resolverNotes(debug: CompilerResolveDebug): string[] {
  const diagnosticPriority = (code: string): number => {
    if (code.includes("output") || code.includes("assembly")) {
      return 0;
    }
    return code === "input-candidate-rejected" ? 2 : 1;
  };
  const diagnostics = [...debug.diagnostics]
    .sort(
      (left, right) =>
        diagnosticPriority(left.code) - diagnosticPriority(right.code),
    )
    .map((diagnostic) => {
      const subject =
        diagnostic.path === undefined ? "" : `${diagnostic.path}: `;
      return `${subject}${diagnostic.message}`;
    });

  const notes: string[] = [];
  for (const diagnostic of diagnostics) {
    if (!notes.includes(diagnostic)) {
      notes.push(diagnostic);
    }
    if (notes.length === 3) {
      break;
    }
  }
  return notes;
}

function compilerOutputs(analysis: CompilerAnalysis): string[] {
  const inferred = new Map(
    analysis.debug?.inferredWrites.map(
      (write) => [write.path, write.reason] as const,
    ),
  );
  return analysis.writes.map((output) => {
    const reason = inferred.get(output);
    if (reason === "default-output") {
      return `${output} (inferred from compiler defaults)`;
    }
    if (reason === "assembly-listing") {
      return `${output} (inferred assembly listing)`;
    }
    return output;
  });
}

function compilerAnalysisSuccessLog(
  id: number,
  command: service.CommandData,
  analysis: CompilerAnalysis,
): string {
  const lines = [
    `Successfully analyzed command #${id}`,
    `  at ${command.cwd}`,
    `    ${commandLine(command)}`,
    ...indentedList("Sources", analysis.sourceFiles),
    ...indentedList("Outputs", compilerOutputs(analysis)),
  ];
  if (analysis.debug !== undefined) {
    const notes = resolverNotes(analysis.debug);
    if (notes.length !== 0) {
      lines.push("  Resolver notes:", ...notes.map((note) => `    ${note}`));
    }
  }

  return lines.join("\n");
}

function compilerAnalysisErrorLog(
  id: number,
  command: service.CommandData,
  error: CompilerAnalysisError,
): string {
  return [
    `Failed to analyze command #${id}`,
    `  at ${command.cwd}`,
    `    ${commandLine(command)}`,
    `  ${error.kind}:`,
    `    ${error.message}`,
  ].join("\n");
}

/**
 * Creates a service script that captures compiler leaf commands and writes a
 * `compile_commands.json` file.
 *
 * Compiler commands contribute artifact links, and source leafs are turned into
 * compilation database entries at the end of the run.
 *
 * When one command feeds another, the saved `output` field points to the
 * current producing command output rather than the final top-level target.
 *
 * Example saved entry:
 * ```json
 * [
 *   {
 *     "directory": "/tmp/demo",
 *     "file": "src/main.cc",
 *     "arguments": ["clang++", "-c", "src/main.cc", "-o", "obj/main.o"],
 *     "output": "/tmp/demo/obj/main.o"
 *   }
 * ]
 * ```
 *
 * Output:
 * ```txt
 * CDB saved to /tmp/demo/build/compile_commands.json with 1 entries.
 * ```
 */
function cdb(
  savePath = "build/compile_commands.json",
): service.CatterContextService {
  let options = defaultOptions(savePath);
  let compilerAnalyzer = new CompilerAnalyzer();
  const commandTree = new data.FlatTree<string, string>();
  const producers = new Map<string, Producer[]>();
  const srcFiles = new Map<string, string>();
  const capturedCompilerCommandIds = new Set<number>();

  function generatedItems(): CDBItem[] {
    commandTree.assemble();

    const items: CDBItem[] = [];
    for (const node of commandTree.nodes()) {
      if (node.children.length !== 0) {
        continue;
      }

      const file = srcFiles.get(node.id);
      if (file === undefined) {
        continue;
      }

      for (const parent of node.parent) {
        const parents = producers.get(parent);
        if (parents === undefined) {
          continue;
        }

        const entries: CDBEntry[] = [
          {
            file,
            output: parent,
          },
        ];

        for (const producer of parents) {
          items.push(...cdbItemsOf(producer, entries));
        }
      }
    }

    return items;
  }

  function save(): void {
    const items = generatedItems();
    verboseLog(
      options,
      `Generated ${items.length} entries for ${new Set(items.map((item) => item.file)).size} source files; ` +
        `${items.filter((item) => item.output !== undefined).length} entries include an output path.`,
    );

    const manager = new CDBManager(options.outputPath, {
      inherit: options.append,
    });
    manager.merge(items);

    const savedPath = manager.save();
    log(
      options,
      `CDB saved to ${fs.path.absolute(savedPath)} with ${manager.items().length} entries.`,
    );
  }

  return service.create({
    onStart(config) {
      const parsed = cli.run(cdbCLI, config.scriptArgs);
      if (parsed === undefined) {
        config.execute = false;
        return config;
      }
      if (parsed.append && parsed.replace) {
        throw new Error("cdb: --append and --replace cannot be used together");
      }

      options = {
        outputPath: parsed.output ?? parsed.path ?? savePath,
        append: parsed.replace ? false : true,
        saveOnFailure: parsed["save-on-failure"],
        abortOnCommandFailure: parsed["abort-on-command-failure"],
        abortOnCaptureError: parsed["abort-on-capture-error"],
        quiet: parsed.quiet,
        verbose: parsed.verbose,
      };
      compilerAnalyzer = new CompilerAnalyzer({
        resolver: new CompilerResolver({ debug: options.verbose }),
      });

      verboseLog(
        options,
        [
          "CDB verbose logging enabled.",
          `  Output file: ${options.outputPath}`,
          `  Existing entries: ${options.append ? "merge" : "replace"}`,
          "  Resolver diagnostics: enabled",
        ].join("\n"),
      );

      return config;
    },

    onFinish(result) {
      if (result.code !== 0 && !options.saveOnFailure) {
        log(
          options,
          `Build failed with exit code ${result.code}. CDB will not be saved.`,
        );
        return;
      }

      save();
    },

    onCommand(ctx) {
      const data = ctx.capture;
      if (!data.success) {
        const message = `CDB received capture error: ${data.error.msg}`;
        if (options.abortOnCaptureError) {
          throw new Error(message);
        }
        log(options, message);
        return;
      }

      const command = data.data;
      const analysisResult = compilerAnalyzer.analyze({
        exe: command.exe,
        argv: command.argv,
      });
      if (analysisResult.isErr()) {
        verboseLog(
          options,
          compilerAnalysisErrorLog(ctx.id, command, analysisResult.error),
        );
        return;
      }

      const analysis = analysisResult.value;
      verboseLog(
        options,
        compilerAnalysisSuccessLog(ctx.id, command, analysis),
      );
      capturedCompilerCommandIds.add(ctx.id);

      for (const source of analysis.sourceFiles) {
        const full = pathOf(command.cwd, source);
        if (full !== undefined) {
          srcFiles.set(full, source);
        }
      }

      for (const edge of analysis.edges) {
        const output = pathOf(command.cwd, edge.output);
        if (output === undefined) {
          continue;
        }

        const inputs = edge.inputs
          .map((input) => pathOf(command.cwd, input))
          .filter(isSet);

        commandTree.justMergeNode({
          id: output,
          content: output,
        });

        for (const input of inputs) {
          commandTree.justMergeNode({
            id: input,
            parent: [output],
            content: input,
          });
        }

        const parents = producers.get(output) ?? [];
        parents.push({
          cwd: command.cwd,
          argv: [...command.argv],
        });
        producers.set(output, parents);
      }

      ctx.ignoreDescendants();
    },

    onExecution(ctx) {
      if (!options.abortOnCommandFailure || ctx.result.code === 0) {
        return;
      }

      const compilerPrefix = capturedCompilerCommandIds.has(ctx.id)
        ? "compiler "
        : "";
      if (options.saveOnFailure) {
        save();
      }
      throw new Error(
        `CDB aborting after ${compilerPrefix}command ${ctx.id} exited with code ${ctx.result.code}.`,
      );
    },
  });
}

service.register(cdb());
