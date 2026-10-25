// Edits the compiles alone, and asks what catter makes of a command of its
// own once the session starts.
import { serve } from "@catter/sdk";

await serve({
  decides: true,
  analysis: true,
  filter: [{ kind: "compiler", phases: ["compile"] }],
  async onStart(_session, catter) {
    const [analysis] = await catter.analyze({ executable: "clang", args: ["clang", "-c", "b.c"] });
    console.log(`analyzed: ${analysis?.kind}`);
  },
  onCommand(command) {
    const analysis = command.analysis;
    if (analysis?.kind !== "compiler") {
      return undefined;
    }
    console.log(`command ${command.id}: ${analysis.phase}, reads ${analysis.reads.map((read) => read.path).join(" ")}`);
    return {
      action: "edit",
      edits: [
        { kind: "remove", flags: ["-DOLD"] },
        { kind: "append", flags: ["-DNEW"] },
      ],
    };
  },
});
