/// The ecosystem's command line (ecosystem/README.md, "Running").
///
///   node tools/ecosystem/run.ts --list
///   node tools/ecosystem/run.ts <project>/<build> --dir <dir> [--out <dir>]
///       [--backend <backend>]... [--recorder auditd|strace|none]
///   node tools/ecosystem/run.ts round <dir of results> [--out <file>]

import * as fs from "node:fs";
import * as path from "node:path";
import { parseArgs } from "node:util";
import { defaultJobs, runJob, type JobResult } from "./job.ts";
import { BACKENDS, listJobs } from "./project.ts";
import { RECORDERS } from "./record/recorder.ts";
import { isRed, jobReport, roundReport } from "./report.ts";

const { values, positionals } = parseArgs({
    allowPositionals: true,
    options: {
        list: { type: "boolean" },
        dir: { type: "string" },
        out: { type: "string" },
        backend: { type: "string", multiple: true },
        recorder: { type: "string" },
    },
});

function choice<T extends string>(value: string, choices: readonly T[], what: string): T {
    if (!(choices as readonly string[]).includes(value)) {
        throw new Error(`no ${what} ${value}: ${choices.join(", ")}`);
    }
    return value as T;
}

/// Every results.json under `dir`, as the round's artifacts unpack.
function findResults(dir: string): JobResult[] {
    return fs
        .readdirSync(dir, { recursive: true, encoding: "utf8" })
        .filter((file) => path.basename(file) === "results.json")
        .sort()
        .map((file) => JSON.parse(fs.readFileSync(path.join(dir, file), "utf8")) as JobResult);
}

const [first, second] = positionals;
if (values.list === true) {
    console.log(JSON.stringify(listJobs()));
} else if (first === "round" && second !== undefined) {
    const report = roundReport(findResults(second));
    if (values.out === undefined) {
        console.log(report);
    } else {
        fs.writeFileSync(values.out, report);
    }
} else if (first !== undefined && values.dir !== undefined) {
    const catter = process.env["CATTER_EXECUTABLE"];
    if (catter === undefined || !fs.existsSync(catter)) {
        throw new Error("CATTER_EXECUTABLE names no catter");
    }
    const dir = path.resolve(values.dir);
    const out = path.resolve(values.out ?? path.join(dir, "out", ...first.split("/")));
    fs.mkdirSync(out, { recursive: true });
    const result = await runJob(first, {
        dir,
        out,
        catter: path.resolve(catter),
        recorder: choice(values.recorder ?? (process.platform === "linux" ? "auditd" : "none"), RECORDERS, "recorder"),
        backends: (values.backend ?? []).map((each) => choice(each, BACKENDS, "backend")),
        jobs: defaultJobs(),
    });
    fs.writeFileSync(path.join(out, "results.json"), JSON.stringify(result, null, 1));
    const report = jobReport(result);
    fs.writeFileSync(path.join(out, "report.md"), report);
    console.log(report);
    process.exitCode = isRed(result) ? 1 : 0;
} else {
    console.error(
        [
            "usage: node tools/ecosystem/run.ts --list",
            "       node tools/ecosystem/run.ts <project>/<build> --dir <dir> [--out <dir>] [--backend <backend>]... [--recorder auditd|strace|none]",
            "       node tools/ecosystem/run.ts round <dir of results> [--out <file>]",
        ].join("\n"),
    );
    process.exitCode = 2;
}
