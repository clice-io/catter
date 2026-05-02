import * as data from "../data/index.js";
import * as service from "../service.js";

import * as cli from "../cli/index.js";
import * as io from "../io.js";
import * as fs from "../fs.js";
import {
  CDBManager,
  type CDBCommand,
  type CDBEntry,
  type CDBItem,
  CompilerAnalysis,
  analyze as analyzeCmd,
  cdbItemsOf,
} from "../cmd/index.js";

type Producer = CDBCommand;

type CDBScriptOptions = {
  outputPath: string;
  append: boolean;
  saveOnFailure: boolean;
  abortOnCommandFailure: boolean;
  abortOnCaptureError: boolean;
  quiet: boolean;
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
  };
}

function log(options: CDBScriptOptions, message: string): void {
  if (!options.quiet) {
    io.println(message);
  }
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
 * @example
 * ```ts
 * import { scripts, service } from "catter";
 *
 * service.register(scripts.cdb("build/compile_commands.json"));
 * ```
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
export function cdb(
  savePath = "build/compile_commands.json",
): service.CatterContextService {
  let options = defaultOptions(savePath);
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
    const manager = new CDBManager(options.outputPath, {
      inherit: options.append,
    });
    manager.merge(generatedItems());

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
      };

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
      const analysis = CompilerAnalysis.from(analyzeCmd(command.argv));
      if (analysis === undefined) {
        return;
      }
      capturedCompilerCommandIds.add(ctx.id);

      for (const input of analysis.inputEntries()) {
        if (input.kind === "source") {
          const full = pathOf(command.cwd, input.path);
          if (full !== undefined) {
            srcFiles.set(full, input.path);
          }
        }
      }

      for (const edge of analysis.edges()) {
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
