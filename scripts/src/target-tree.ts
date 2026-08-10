import { path } from "catter/fs";
import { create, register, type CatterContextService } from "catter/service";
import * as cli from "catter/cli";
import { println, print } from "catter/io";
import { FlatTree } from "catter/data";
import { TreeRenderer } from "catter/view";
import {
  ArchiverAnalyzer,
  CompilerAnalyzer,
  Registry,
  type CommandAnalysis,
  type CommandAnalyzerError,
} from "catter/cmd";

function isDefined<T>(value: T | undefined): value is T {
  return value !== undefined;
}

function normalizePath(cwd: string, pathStr: string): string | undefined {
  if (pathStr === "-") {
    return undefined;
  }

  const base = path.absolute(cwd);
  const joined = path.isAbsolute(pathStr)
    ? pathStr
    : path.joinAll(base, pathStr);
  return path.lexicalNormal(joined);
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
 * `analysis.edges`, and the final output is rendered with `FlatTree`.
 *
 * Output:
 * ```txt
 * .
 * └── app
 *     ├── main.o
 *     └── util.o
 * ```
 */
function targetTree(): CatterContextService {
  const targetTree = new FlatTree<string, string>();
  let maxDepth: number | undefined;
  const analyzerRegistry = new Registry<CommandAnalysis, CommandAnalyzerError>()
    .register("compiler", new CompilerAnalyzer())
    .register("archiver", new ArchiverAnalyzer());
  return create({
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
        println(
          `Build failed with exit code ${result.code}. Printing partial target forest.`,
        );
      }

      if (targetTree.size() === 0) {
        println("No targets found.");
        return;
      }

      const cycles = targetTree.assemble();
      const walker = targetTree.walk();
      const renderer = new TreeRenderer({
        first: walker.first,
        children: walker.children,
        content: (id) => targetTree.node(id)?.content,
      });

      print(
        renderer.output({
          type: "cli",
          maxDepth,
          text: (_content, id) => path.filename(id) || id,
        }),
      );

      if (cycles.length > 0) {
        println("");
        println("Detected target cycles:");
        for (const cycle of cycles) {
          const names = cycle.map((id) => path.filename(id) || id);
          println(`[cycle] ${names.join(" -> ")} -> ${names[0]}`);
        }
      }
    },

    onCommand(ctx) {
      const data = ctx.capture;
      if (data.isErr()) {
        return;
      }

      const analysisResult = analyzerRegistry.analyze({
        exe: data.value.exe,
        argv: data.value.argv,
      });
      if (analysisResult.isErr()) {
        return;
      }

      const analysis = analysisResult.value;
      const targetEntries = analysis.edges;
      const entries = targetEntries
        .map((entry) => {
          const output = normalizePath(data.value.cwd, entry.output);
          if (output === undefined) {
            return undefined;
          }

          return {
            output,
            inputs: entry.inputs
              .map((input) => normalizePath(data.value.cwd, input))
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

      ctx.ignoreDescendants();
    },
  });
}

register(targetTree());
