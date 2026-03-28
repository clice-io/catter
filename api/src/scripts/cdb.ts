import * as service from "../service.js";

import * as io from "../io.js";
import * as fs from "../fs.js";
import {
  CDBManager,
  type CDBItem,
  CompilerAnalysis,
  analyze as analyzeCmd,
  cdbItemsOf,
} from "../cmd/index.js";

/**
 * Service script that captures compiler leaf commands and writes a
 * `compile_commands.json` file.
 *
 * Only compiler commands that represent source-to-object compilation contribute
 * entries. Other commands are ignored.
 *
 * @example
 * ```ts
 * import { scripts, service } from "catter";
 *
 * service.register(new scripts.CDB("build/compile_commands.json"));
 * ```
 */
export class CDB extends service.IgnorableService {
  /** Destination path used when saving the compilation database. */
  save_path: string;
  private readonly generatedItems: CDBItem[] = [];

  /**
   * Creates a CDB script service.
   *
   * @example
   * ```ts
   * const cdb = new scripts.CDB("build/compile_commands.json");
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

  override onFinish(event: service.ExecutionEvent) {
    if (event.code !== 0) {
      io.println(
        `Build failed with exit code ${event.code}. CDB will not be saved.`,
      );
      return;
    }

    const manager = new CDBManager(this.save_path);
    manager.merge(this.generatedItems);

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
      return {
        type: "skip",
      };
    }

    const result = analyzeCmd(data.data.argv);
    const analysis = CompilerAnalysis.from(result);
    if (analysis === undefined) {
      return {
        type: "skip",
      };
    }

    this.generatedItems.push(...cdbItemsOf(data.data, analysis));
    return {
      type: "ignore",
    };
  }
}
