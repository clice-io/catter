import * as data from "../data/index.js";
import * as fs from "../fs.js";
import * as io from "../io.js";
import * as service from "../service.js";
import {
  CompilerCmdAnalysis,
  CompilerCommandType,
  type CompilerCommandType as CompilerCommandTypeValue,
} from "../cmd/index.js";

type TargetEntry = {
  output: string;
  inputs: string[];
  type: CompilerCommandTypeValue;
};

function isDefined<T>(value: T | undefined): value is T {
  return value !== undefined;
}

function looksAbsolutePath(path: string): boolean {
  return (
    path.startsWith("/") ||
    path.startsWith("\\\\") ||
    /^[A-Za-z]:[\\/]/.test(path)
  );
}

function normalizeCommandPath(cwd: string, path: string): string | undefined {
  if (path === "-") {
    return undefined;
  }

  const base = fs.path.absolute(cwd);
  const joined = looksAbsolutePath(path) ? path : fs.path.joinAll(base, path);
  return fs.path.lexicalNormal(joined);
}

function isTargetType(
  type: CompilerCommandTypeValue | undefined,
): type is CompilerCommandTypeValue {
  switch (type) {
    case CompilerCommandType.SourceToObject:
    case CompilerCommandType.SourceToExe:
    case CompilerCommandType.ObjectToExe:
    case CompilerCommandType.ObjectToShare:
    case CompilerCommandType.ObjectToLib:
    case CompilerCommandType.RelocatableLink:
      return true;
    default:
      return false;
  }
}

function targetEntriesFromCommand(command: service.CommandData): TargetEntry[] {
  const analysis = new CompilerCmdAnalysis(command.argv);
  const type = analysis.type;
  if (!isTargetType(type)) {
    return [];
  }

  const inputs = analysis
    .inputs()
    .map((input) => normalizeCommandPath(command.cwd, input))
    .filter(isDefined);
  const outputs = analysis
    .outputs()
    .map((output) => normalizeCommandPath(command.cwd, output))
    .filter(isDefined);

  if (outputs.length === 0) {
    return [];
  }

  if (type === CompilerCommandType.SourceToObject) {
    if (outputs.length === inputs.length) {
      return outputs.map((output, index) => ({
        output,
        inputs: [inputs[index]],
        type,
      }));
    }

    if (outputs.length === 1 && inputs.length === 1) {
      return [
        {
          output: outputs[0],
          inputs: [inputs[0]],
          type,
        },
      ];
    }

    return [];
  }

  return outputs.map((output) => ({
    output,
    inputs: [...inputs],
    type,
  }));
}

function ensureTargetNode(
  tree: data.FlatTree<string, string>,
  id: string,
): void {
  const parentId = tree.node(id)?.parentId ?? null;
  tree.set({
    id,
    parentId,
    content: id,
  });
}

function attachTargetNode(
  tree: data.FlatTree<string, string>,
  childId: string,
  parentId: string,
): void {
  const currentNode = tree.node(childId);
  const nextParentId =
    currentNode === undefined || currentNode.parentId === null
      ? parentId
      : currentNode.parentId;

  tree.set({
    id: childId,
    parentId: nextParentId,
    content: childId,
  });
}

function recordTargetEntries(
  tree: data.FlatTree<string, string>,
  entries: TargetEntry[],
): void {
  for (const entry of entries) {
    ensureTargetNode(tree, entry.output);
    for (const input of entry.inputs) {
      attachTargetNode(tree, input, entry.output);
    }
  }
}

function parseDepth(args: string[]): number | undefined {
  let depth: number | undefined;

  for (let index = 0; index < args.length; ++index) {
    const arg = args[index];
    switch (arg) {
      case "-d":
      case "--depth": {
        const value = args[index + 1];
        if (value === undefined) {
          throw new Error(`target-tree: missing value for ${arg}`);
        }

        const parsed = Number(value);
        if (!Number.isInteger(parsed) || parsed < 0) {
          throw new Error(
            `target-tree: depth must be a non-negative integer, got ${value}`,
          );
        }

        depth = parsed;
        ++index;
        break;
      }
      default:
        throw new Error(`target-tree: unsupported script arg: ${arg}`);
    }
  }

  return depth;
}

export class TargetTree extends service.CompilerCmdService {
  private readonly targetTree = new data.FlatTree<string, string>();
  private maxDepth: number | undefined;

  override onStart(config: service.CatterConfig): service.CatterConfig {
    this.maxDepth = parseDepth(config.scriptArgs);
    return config;
  }

  override onFinish(event: service.ExecutionEvent) {
    if (event.code !== 0) {
      io.println(
        `Build failed with exit code ${event.code}. Printing partial target forest.`,
      );
    }

    if (this.targetTree.size === 0) {
      io.println("No binary targets found.");
      return;
    }

    io.print(
      this.targetTree.render({
        stringify: (_content, node) => fs.path.filename(node.id) || node.id,
        maxDepth: this.maxDepth,
      }),
    );
  }

  override onCommand(
    _id: number,
    data: service.CommandCaptureResult,
  ): service.IgnorableAction {
    recordTargetEntries(
      this.targetTree,
      targetEntriesFromCommand(this.compilerCommandData(data).data),
    );

    return {
      type: "skip",
    };
  }
}
