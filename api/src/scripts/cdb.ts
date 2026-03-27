import * as service from "../service.js";

import * as io from "../io.js";
import * as fs from "../fs.js";
import { CDBManager, type CDBItem, CompilerCmdAnalysis } from "../cmd/index.js";

function itemsFromCommand(command: service.CommandData): CDBItem[] {
  return new CompilerCmdAnalysis(command.argv)
    .compilationDatabaseEntries()
    .map((entry) => {
      const item: CDBItem = {
        directory: command.cwd,
        file: entry.file,
        arguments: [...command.argv],
      };

      if (entry.output !== undefined) {
        item.output = entry.output;
      }

      return item;
    });
}

export class CDB extends service.IgnorableService {
  save_path: string;
  private readonly generatedItems: CDBItem[] = [];

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
    id: number,
    data: service.CommandCaptureResult,
  ): service.IgnorableAction {
    if (!data.success) {
      io.println(`CDB received error: ${data.error.msg}`);
      return {
        type: "skip",
      };
    }

    if (CompilerCmdAnalysis.isSupport(data.data.argv)) {
      this.generatedItems.push(...itemsFromCommand(data.data));
      return {
        type: "ignore",
      };
    }

    return {
      type: "skip",
    };
  }
}
