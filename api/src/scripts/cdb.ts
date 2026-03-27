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

export class CDB implements service.CatterService {
  save_path: string;
  commandArray: service.CommandData[] = [];

  constructor(save_path?: string) {
    this.save_path = save_path ?? "compile_commands.json";
  }

  onStart(config: service.CatterConfig): service.CatterConfig {
    if (config.scriptArgs.length > 0) {
      this.save_path = config.scriptArgs[0];
    }
    return config;
  }

  onFinish(event: service.ExecutionEvent) {
    if (event.code !== 0) {
      io.println(
        `Build failed with exit code ${event.code}. CDB will not be saved.`,
      );
      return;
    }

    const manager = new CDBManager(this.save_path);
    for (const command of this.commandArray) {
      manager.merge(itemsFromCommand(command));
    }

    const savedPath = manager.save();
    io.println(
      `CDB saved to ${fs.path.absolute(savedPath)} with ${manager.items().length} entries.`,
    );
  }

  onCommand(id: number, data: service.CommandCaptureResult): service.Action {
    if (!data.success) {
      io.println(`CDB received error: ${data.error.msg}`);
    } else if (CompilerCmdAnalysis.isSupport(data.data.argv)) {
      this.commandArray.push(data.data);
    }

    return {
      type: "skip",
    };
  }

  onExecution(id: number, event: service.ExecutionEvent) {
    // No action needed for execution events in this service.
  }
}
