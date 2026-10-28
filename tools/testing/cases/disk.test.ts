import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { afterEach, describe, expect, test } from "vitest";
import { StepRecord } from "../steps.ts";
import { Disk } from "./disk.ts";

describe("Disk.trace", () => {
    const dirs: string[] = [];
    afterEach(() => {
        for (const dir of dirs.splice(0)) {
            fs.rmSync(dir, { recursive: true, force: true });
        }
    });

    function disk(files: Record<string, string>): Disk {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), "catter-disk-"));
        dirs.push(root);
        for (const [name, text] of Object.entries(files)) {
            fs.writeFileSync(path.join(root, name), text);
        }
        return new Disk(root, new StepRecord(), (text) => text);
    }

    function metadata(name: string, args: Record<string, unknown>, tid?: number): unknown {
        return { name, ph: "M", pid: 1, ...(tid === undefined ? {} : { tid }), args };
    }

    function command(tid: number, name: string, ts: number, dur: number, id: number): unknown {
        return { name, ph: "X", pid: 1, tid, ts, dur, args: { exit: 0, id } };
    }

    test("shows each lane's commands nested, without their times or the critical path", () => {
        const trace = {
            traceEvents: [
                metadata("process_name", { name: "make" }),
                metadata("thread_name", { name: "critical path" }, 1),
                metadata("thread_name", { name: "lane 2" }, 3),
                metadata("thread_name", { name: "lane 1" }, 2),
                command(1, "cc a.c", 5, 30, 3),
                command(3, "cc b.c", 20, 10, 4),
                command(2, "sh", 5, 30, 2),
                command(2, "make", 0, 100, 1),
                { ...(command(2, "cc a.c", 5, 30, 3) as object), cat: "compile" },
                command(2, "cc1", 10, 0, 5),
                command(2, "ld", 35, 5, 6),
                { name: "running", ph: "C", pid: 1, ts: 20, args: { commands: 2 } },
                { name: "running", ph: "C", pid: 1, ts: 0, args: { commands: 1 } },
            ],
        };
        expect(disk({ "trace.json": JSON.stringify(trace) }).trace()).toEqual({
            process: "make",
            "lane 1": [
                { depth: 0, name: "make", exit: 0, id: 1 },
                { depth: 1, name: "sh", exit: 0, id: 2 },
                { depth: 2, name: "cc a.c", cat: "compile", exit: 0, id: 3 },
                { depth: 3, name: "cc1", exit: 0, id: 5 },
                { depth: 1, name: "ld", exit: 0, id: 6 },
            ],
            "lane 2": [{ depth: 0, name: "cc b.c", exit: 0, id: 4 }],
            running: [1, 2],
        });
    });

    test("shows a missing trace as absent", () => {
        expect(disk({}).trace()).toEqual({ absent: true });
    });
});
