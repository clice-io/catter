import * as cli from "../cli/index.js";
import * as data from "../data/index.js";
import * as io from "../io.js";
import * as service from "../service.js";
import * as view from "../view/index.js";

const cmdTreeCLI = cli.command({
  name: "cmd-tree",
  description: "Render captured commands as a command tree.",
  options: [
    cli.number("depth", {
      short: "d",
      valueName: "n",
      description: "Limit render depth.",
      integer: true,
      min: 0,
    }),
    cli.number("args", {
      short: "a",
      valueName: "n",
      description:
        "Show the first n arguments after argv[0]. Use -1 to keep all.",
      integer: true,
      min: -1,
      default: -1,
    }),
    cli.number("argWidth", {
      short: "w",
      valueName: "n",
      description: "Limit the display width of each argument.",
      integer: true,
      min: 4,
      default: 10,
    }),
  ] as const,
});

function truncateMiddle(text: string, maxWidth: number): string {
  if (maxWidth < 0 || text.length <= maxWidth) {
    return text;
  }

  if (maxWidth <= 4) {
    return ".".repeat(maxWidth);
  }

  const remain = maxWidth - 4;
  const left = Math.ceil(remain / 2);
  const right = Math.floor(remain / 2);
  return `${text.slice(0, left)}....${text.slice(text.length - right)}`;
}

function quoteArg(text: string, preserveQuoted = false): string {
  if (text.length === 0) {
    return '""';
  }

  if (preserveQuoted || /[\s"'\\]/.test(text)) {
    return JSON.stringify(text);
  }

  return text;
}

function formatCommand(
  argv: readonly string[],
  visibleArgCount: number,
  maxArgWidth: number,
): string {
  if (argv.length === 0) {
    return "<empty command>";
  }

  const head = argv[0];
  const tail =
    visibleArgCount < 0 ? argv.slice(1) : argv.slice(1, visibleArgCount + 1);
  const clipped = visibleArgCount >= 0 && argv.length > tail.length + 1;
  const visible = [head, ...tail].map((part) =>
    quoteArg(truncateMiddle(part, maxArgWidth), /[\s"'\\]/.test(part)),
  );

  if (clipped) {
    visible.push("...");
  }

  return visible.join(" ");
}

/**
 * Service script that renders the captured command DAG as a tree of full
 * commands.
 *
 * Each node is printed from the captured `argv`, with optional argument count
 * and per-argument width limits.
 *
 * @example
 * ```ts
 * import { scripts, service } from "catter";
 *
 * service.register(scripts.cmdTree());
 * ```
 *
 * Output:
 * ```txt
 * .
 * └── clang++ main.cc -c
 *     └── ld main.o -o app
 * ```
 */
export function cmdTree(): service.CatterContextService {
  const commandTree = new data.FlatTree<number, service.CommandCaptureResult>();
  let maxDepth: number | undefined;
  let visibleArgCount = -1;
  let maxArgWidth = 10;

  return service.create({
    onCommand(ctx) {
      const capture = ctx.capture;
      commandTree.justMergeNode({
        id: ctx.id,
        parent:
          capture.success && capture.data.parent !== undefined
            ? [capture.data.parent]
            : [],
        content: capture,
      });
    },

    onStart(config) {
      const res = cli.run(cmdTreeCLI, config.scriptArgs);
      if (res === undefined) {
        config.execute = false;
        return config;
      }

      maxDepth = res.depth;
      visibleArgCount = res.args ?? -1;
      maxArgWidth = res.argWidth ?? 10;
      return config;
    },

    onFinish(result) {
      if (result.code !== 0) {
        io.println(
          `Build failed with exit code ${result.code}. Printing partial command tree.`,
        );
      }

      if (commandTree.size() === 0) {
        io.println("No commands found.");
        return;
      }
      commandTree.assemble();

      const walker = commandTree.walk();
      const renderer = new view.TreeRenderer({
        first: walker.first,
        children: walker.children,
        content: (id) => commandTree.node(id)?.content,
      });

      io.print(
        renderer.output({
          type: "cli",
          maxDepth,
          text: (capture) => {
            if (!capture.success) {
              return `[capture error] ${capture.error.msg}`;
            }

            return formatCommand(
              capture.data.argv,
              visibleArgCount,
              maxArgWidth,
            );
          },
        }),
      );
    },
  });
}
