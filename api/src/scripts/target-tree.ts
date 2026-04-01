import * as data from "../data/index.js";
import * as fs from "../fs.js";
import * as io from "../io.js";
import * as service from "../service.js";
import * as cli from "../cli/index.js";
import * as view from "../view/index.js";
import { analyze as analyzeCmd } from "../cmd/index.js";

type TreeState<Id extends PropertyKey, Content> = {
  dataPool: Map<Id, data.FlatTreeNodeStore<Id, Content>>;
  mergeNode: (node: {
    id: Id;
    content: Content;
    parent?: Id[];
    children?: Id[];
  }) => void;
};

function treeState<Id extends PropertyKey, Content>(
  tree: data.FlatTree<Id, Content>,
): TreeState<Id, Content> {
  return tree as unknown as TreeState<Id, Content>;
}

function treeContent<Id extends PropertyKey, Content>(
  tree: data.FlatTree<Id, Content>,
  id: Id,
): Content | undefined {
  return treeState(tree).dataPool.get(id)?.content;
}

function treeSize<Id extends PropertyKey, Content>(
  tree: data.FlatTree<Id, Content>,
): number {
  return treeState(tree).dataPool.size;
}

function mergeTreeNode<Id extends PropertyKey, Content>(
  tree: data.FlatTree<Id, Content>,
  node: {
    id: Id;
    content: Content;
    parent?: Id[];
    children?: Id[];
  },
): void {
  treeState(tree).mergeNode(node);
}

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

function normalizePath(cwd: string, path: string): string | undefined {
  if (path === "-") {
    return undefined;
  }

  const base = fs.path.absolute(cwd);
  const joined = looksAbsolutePath(path) ? path : fs.path.joinAll(base, path);
  return fs.path.lexicalNormal(joined);
}

const targetTreeCLI = cli.command({
  name: "target-tree",
  description: "Render captured build targets as a flat target forest.",
  options: [
    cli.number("depth", {
      short: "d",
      valueName: "n",
      description: "Limit render depth.",
      integer: true,
      min: 0,
    }),
  ] as const,
});

/**
 * Service script that renders the captured command products as a dependency
 * forest.
 *
 * Each recognized command contributes dependency edges through
 * `analysis.edges()`, and the final output is rendered with `FlatTree`.
 *
 * @example
 * ```ts
 * import { scripts, service } from "catter";
 *
 * service.register(new scripts.TargetTree());
 * ```
 */
export class TargetTree extends service.IgnorableService {
  private readonly targetTree = new data.FlatTree<string, string>();
  private maxDepth: number | undefined;

  override onStart(config: service.CatterConfig): service.CatterConfig {
    const res = cli.run(targetTreeCLI, config.scriptArgs);
    if (res) {
      this.maxDepth = res.depth;
      return config;
    }
    config.execute = false;
    return config;
  }

  override onFinish(event: service.ExecutionEvent) {
    if (event.code !== 0) {
      io.println(
        `Build failed with exit code ${event.code}. Printing partial target forest.`,
      );
    }

    if (treeSize(this.targetTree) === 0) {
      io.println("No targets found.");
      return;
    }

    const walker = this.targetTree.walk();
    const renderer = new view.TreeRenderer<string, string>({
      first: walker.first,
      children: walker.children,
      content: (id) => treeContent(this.targetTree, id),
    });

    io.print(
      renderer.output({
        type: "cli",
        maxDepth: this.maxDepth,
        text: (_content, id) => fs.path.filename(id) || id,
      }),
    );
  }

  override onCommand(
    _id: number,
    data: service.CommandCaptureResult,
  ): service.IgnorableAction {
    if (!data.success) {
      return {
        type: "skip",
      };
    }

    const analysis = analyzeCmd(data.data.argv);
    const targetEntries = analysis?.edges() ?? [];
    const entries = targetEntries
      .map((entry) => {
        const output = normalizePath(data.data.cwd, entry.output);
        if (output === undefined) {
          return undefined;
        }

        return {
          output,
          inputs: entry.inputs
            .map((input) => normalizePath(data.data.cwd, input))
            .filter(isDefined),
        };
      })
      .filter(isDefined);

    for (const entry of entries) {
      mergeTreeNode(this.targetTree, {
        id: entry.output,
        content: entry.output,
      });
      for (const input of entry.inputs) {
        mergeTreeNode(this.targetTree, {
          id: input,
          parent: [entry.output],
          content: input,
        });
      }
    }

    if (analysis !== undefined) {
      return {
        type: "ignore",
      };
    }

    return {
      type: "skip",
    };
  }
}
