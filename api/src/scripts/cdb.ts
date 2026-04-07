import * as data from "../data/index.js";
import * as service from "../service.js";

import * as io from "../io.js";
import * as fs from "../fs.js";
import {
  CDBManager,
  type CDBCommand,
  type CDBEntry,
  CompilerAnalysis,
  analyze as analyzeCmd,
  cdbItemsOf,
} from "../cmd/index.js";

type Producer = CDBCommand;

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

/**
 * Service script that captures compiler leaf commands and writes a
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
 * service.register(new scripts.CDB("build/compile_commands.json"));
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
export class CDB extends service.IgnorableService {
  /** Destination path used when saving the compilation database. */
  save_path: string;
  private readonly commandTree = new data.FlatTree<string, string>();
  private readonly producers = new Map<string, Producer[]>();
  private readonly srcFiles = new Map<string, string>();

  /**
   * Creates a CDB script service.
   *
   * @example
   * ```ts
   * const cdb = new scripts.CDB("build/compile_commands.json");
   * ```
   *
   * Output path used when omitted:
   * ```txt
   * build/compile_commands.json
   * ```
   */
  constructor(save_path?: string) {
    super();
    this.save_path = save_path ?? "build/compile_commands.json";
  }

  override onStart(config: service.CatterConfig): service.CatterConfig {
    if (config.scriptArgs.length > 0) {
      this.save_path = config.scriptArgs[0];
    }
    return config;
  }

  override onFinish(result: service.ProcessResult) {
    if (result.code !== 0) {
      io.println(
        `Build failed with exit code ${result.code}. CDB will not be saved.`,
      );
      return;
    }

    this.commandTree.assemble();

    const generatedItems = [];
    for (const node of this.commandTree.nodes()) {
      if (node.children.length !== 0) {
        continue;
      }

      const file = this.srcFiles.get(node.id);
      if (file === undefined) {
        continue;
      }

      for (const parent of node.parent) {
        const parents = this.producers.get(parent);
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
          generatedItems.push(...cdbItemsOf(producer, entries));
        }
      }
    }

    const manager = new CDBManager(this.save_path);
    manager.merge(generatedItems);

    const savedPath = manager.save();
    io.println(
      `CDB saved to ${fs.path.absolute(savedPath)} with ${manager.items().length} entries.`,
    );
  }

  override onCommand(
    _id: number,
    data: service.CommandCaptureResult,
  ): service.IgnorableAction {
    if (!data.success) {
      io.println(`CDB received error: ${data.error.msg}`);
      return { type: "skip" };
    }

    const command = data.data;
    const analysis = CompilerAnalysis.from(analyzeCmd(command.argv));
    if (analysis === undefined) {
      return { type: "skip" };
    }

    for (const input of analysis.inputEntries()) {
      if (input.kind === "source") {
        const full = pathOf(command.cwd, input.path);
        if (full !== undefined) {
          this.srcFiles.set(full, input.path);
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

      this.commandTree.justMergeNode({
        id: output,
        content: output,
      });

      for (const input of inputs) {
        this.commandTree.justMergeNode({
          id: input,
          parent: [output],
          content: input,
        });
      }

      const parents = this.producers.get(output) ?? [];
      parents.push({
        cwd: command.cwd,
        argv: [...command.argv],
      });
      this.producers.set(output, parents);
    }

    return { type: "ignore" };
  }
}
