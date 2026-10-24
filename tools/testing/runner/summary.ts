/// Under UPDATE_SNAPSHOTS=1, collect what each case's update changed and
/// print the summary once the run ends.

import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { renderSummary } from "../cases/output.ts";

export default function setup(): (() => void) | undefined {
    if (process.env["UPDATE_SNAPSHOTS"] !== "1") {
        return undefined;
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "catter-snapshot-summary-"));
    process.env["CATTER_SNAPSHOT_SUMMARY_DIR"] = dir;
    return () => {
        const summary = renderSummary(dir);
        console.log(summary === "" ? "outputs unchanged" : summary);
        fs.rmSync(dir, { recursive: true, force: true });
    };
}
