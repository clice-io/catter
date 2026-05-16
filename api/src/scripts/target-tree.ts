import * as data from "../data/index.js";
import * as fs from "../fs.js";
import * as io from "../io.js";
import * as service from "../service.js";
import * as cli from "../cli/index.js";
import * as view from "../view/index.js";
import { analyze as analyzeCmd } from "../cmd/index.js";

function isDefined<T>(value: T | undefined): value is T {
  return value !== undefined;
}

function normalizePath(cwd: string, path: string): string | undefined {
  if (path === "-") {
    return undefined;
  }

  const base = fs.path.absolute(cwd);
  const joined = fs.path.isAbsolute(path) ? path : fs.path.joinAll(base, path);
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
 * service.register(scripts.targetTree());
 * ```
 *
 * Output:
 * ```txt
 * .
 * └── app
 *     ├── main.o
 *     └── util.o
 * ```
 */
export function targetTree(): service.CatterContextService {
  const targetTree = new data.FlatTree<string, string>();
  let maxDepth: number | undefined;

  return service.create({
    onStart(config) {
      const res = cli.run(targetTreeCLI, config.scriptArgs);
      if (res) {
        maxDepth = res.depth;
        return config;
      }
      config.execute = false;
      return config;
    },

    onFinish(result) {
      if (result.code !== 0) {
        io.println(
          `Build failed with exit code ${result.code}. Printing partial target forest.`,
        );
      }

      if (targetTree.size() === 0) {
        io.println("No targets found.");
        return;
      }

      targetTree.assemble();
      const walker = targetTree.walk();
      const renderer = new view.TreeRenderer({
        first: walker.first,
        children: walker.children,
        content: (id) => targetTree.node(id)?.content,
      });

      io.print(
        renderer.output({
          type: "cli",
          maxDepth,
          text: (_content, id) => fs.path.filename(id) || id,
        }),
      );
    },

    onCommand(ctx) {
      const data = ctx.capture;
      if (!data.success) {
        return;
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
        targetTree.justMergeNode({
          id: entry.output,
          content: entry.output,
        });
        for (const input of entry.inputs) {
          targetTree.justMergeNode({
            id: input,
            parent: [entry.output],
            content: input,
          });
        }
      }

      if (analysis !== undefined) {
        ctx.ignoreDescendants();
      }
    },
  });
}
