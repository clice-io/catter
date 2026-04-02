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

function quoteArg(text: string): string {
  if (text.length === 0) {
    return '""';
  }

  if (/[\s"'\\]/.test(text)) {
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
    quoteArg(truncateMiddle(part, maxArgWidth)),
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
 * service.register(new scripts.CmdTree());
 * ```
 */
export class CmdTree extends service.IgnorableService {
  private readonly commandTree = new data.FlatTree<
    number,
    service.CommandCaptureResult
  >();
  private maxDepth?: number;
  private visibleArgCount = -1;
  private maxArgWidth = 10;

  override onCommand(
    id: number,
    capture: service.CommandCaptureResult,
  ): service.Action {
    this.commandTree.justMergeNode({
      id,
      parent:
        capture.success && capture.data.parent !== undefined
          ? [capture.data.parent]
          : [],
      content: capture,
    });
    return {
      type: "skip",
    };
  }

  override onStart(config: service.CatterConfig): service.CatterConfig {
    const res = cli.run(cmdTreeCLI, config.scriptArgs);
    if (res === undefined) {
      config.execute = false;
      return config;
    }

    this.maxDepth = res.depth;
    this.visibleArgCount = res.args ?? -1;
    this.maxArgWidth = res.argWidth ?? 10;
    return config;
  }

  override onFinish(event: service.ExecutionEvent): void {
    if (event.code !== 0) {
      io.println(
        `Build failed with exit code ${event.code}. Printing partial command tree.`,
      );
    }

    if (this.commandTree.size() === 0) {
      io.println("No commands found.");
      return;
    }
    this.commandTree.assemble();

    const walker = this.commandTree.walk();
    const renderer = new view.TreeRenderer({
      first: walker.first,
      children: walker.children,
      content: (id) => this.commandTree.node(id)?.content,
    });

    io.print(
      renderer.output({
        type: "cli",
        maxDepth: this.maxDepth,
        text: (capture) => {
          if (!capture.success) {
            return `[capture error] ${capture.error.msg}`;
          }

          return formatCommand(
            capture.data.argv,
            this.visibleArgCount,
            this.maxArgWidth,
          );
        },
      }),
    );
  }
}
