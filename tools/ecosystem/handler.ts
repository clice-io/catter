/// The handler a job runs catter with: it only observes, and writes every
/// event of the session, a JSON line each, to the file its argument names:
/// each command with its environment and catter's analysis, each capture
/// error, each exit, and the build's end. What it writes is catter's side of
/// every check, and the replay `catter targets --from` reads.
///
///   node tools/ecosystem/handler.ts <events.jsonl>

import * as fs from "node:fs";
import { serve, type OsString } from "@catter/sdk";

const [file] = process.argv.slice(2);
if (file === undefined) {
    throw new Error("usage: handler.ts <events.jsonl>");
}
const out = fs.openSync(file, "w");

function text(value: OsString): string {
    return typeof value === "string" ? value : Buffer.from(value.base64, "base64").toString("utf8");
}

function write(event: object): void {
    fs.writeSync(out, `${JSON.stringify(event)}\n`);
}

await serve({
    env: true,
    analysis: true,
    onCommand(command) {
        write({
            type: "command",
            id: command.id,
            parent: command.parent ?? null,
            cwd: text(command.cwd),
            exe: text(command.executable),
            args: command.args.map(text),
            env: (command.env ?? []).map(text),
            analysis: command.analysis ?? null,
        });
    },
    onError(error) {
        write({ type: "error", ...error, parent: error.parent ?? null });
    },
    onExit(exit) {
        write({ type: "exit", ...exit });
    },
    onFinish(finish) {
        write({ type: "finish", ...finish });
        fs.closeSync(out);
    },
});
