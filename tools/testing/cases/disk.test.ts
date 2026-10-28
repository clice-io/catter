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

    function disk(trace: unknown): Disk {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), "catter-disk-"));
        dirs.push(root);
        fs.writeFileSync(path.join(root, "trace.json"), JSON.stringify(trace));
        return new Disk(root, new StepRecord(), (text) => text);
    }

    function thread(tid: number, name: string): unknown {
        return { name: "thread_name", ph: "M", pid: 1, tid, args: { name } };
    }

    function command(tid: number, name: string, ts: number, dur: number, exit = 0): unknown {
        return { name, ph: "X", pid: 1, tid, ts, dur, args: { exit } };
    }

    test("shows each thread's commands nested, without their times", () => {
        const trace = {
            traceEvents: [
                thread(3, "lane 2"),
                thread(2, "lane 1"),
                command(3, "cc b.c", 20, 10),
                command(2, "sh", 5, 30, 1),
                command(2, "make", 0, 100),
                command(2, "cc a.c", 5, 30, 1),
                command(2, "cc1", 10, 0),
                command(2, "ld", 35, 5),
                { name: "running", ph: "C", pid: 1, ts: 0, args: { commands: 1 } },
            ],
        };
        expect(disk(trace).trace()).toEqual({
            "lane 1": ["make", "  sh exit 1", "    cc a.c exit 1", "      cc1", "  ld"],
            "lane 2": ["cc b.c"],
        });
    });

    test("shows a missing trace as absent", () => {
        const root = fs.mkdtempSync(path.join(os.tmpdir(), "catter-disk-"));
        dirs.push(root);
        expect(new Disk(root, new StepRecord(), (text) => text).trace()).toEqual({ absent: true });
    });
});
