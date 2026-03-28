import * as data from "../data/index.js";
import * as fs from "../fs.js";
import * as io from "../io.js";
import * as service from "../service.js";
import * as cli from "../cli/index.js";
import { analyze as analyzeCmd } from "../cmd/index.js";

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

    if (this.targetTree.size === 0) {
      io.println("No targets found.");
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
      this.targetTree.set({
        id: entry.output,
        content: entry.output,
      });
      for (const input of entry.inputs) {
        this.targetTree.set({
          id: input,
          parentId: entry.output,
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
