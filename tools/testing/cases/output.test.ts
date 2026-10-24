import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { afterEach, describe, expect, test, vi } from "vitest";
import type { Table } from "../render/value.ts";
import type { Case } from "./case.ts";
import {
    Output,
    checkOutput,
    merge,
    readSnapshot,
    referenceOf,
    writeSnapshot,
    type Platform,
    type TestResult,
} from "./output.ts";

function result(
    blocks: Record<string, Table> | undefined,
    reference: Platform = "linux",
): TestResult[] {
    return [
        {
            name: "test",
            reference,
            blocks: blocks === undefined ? undefined : new Map(Object.entries(blocks)),
            excepted: [],
        },
    ];
}

function after(text: string, results: TestResult[], platform: Platform): string {
    return writeSnapshot(merge(readSnapshot(text), results, platform, "t/s/01_c"));
}

const linux = ['[test."$ catter x"]', 'stdout = "ok"', "exit = 0", ""].join("\n");

describe("Output", () => {
    test("labels are unique, a command run again counts", () => {
        const output = new Output();
        output.transcript(output.reserve("$ catter x"), { exit: 0 });
        output.transcript(output.reserve("$ catter x"), { exit: 1 });
        expect([...output.blocks.keys()]).toEqual(["$ catter x", "$ catter x #2"]);
    });

    test("commands side by side keep the order they started in", () => {
        const output = new Output();
        const first = output.reserve("$ catter a");
        const second = output.reserve("$ catter b");
        output.transcript(second, { exit: 0 });
        output.transcript(first, { exit: 0 });
        expect([...output.blocks.keys()]).toEqual(["$ catter a", "$ catter b"]);
    });

    test("snap.toml's own keys are no output's", () => {
        const output = new Output();
        expect(() => {
            output.block("a", { wrong: "x" });
        }).toThrow(/own key/);
        expect(() => {
            output.block("a", { win32: { exit: 0 } });
        }).toThrow(/own key/);
    });
});

describe("platforms", () => {
    test("the reference is the first platform a test runs on", () => {
        expect(referenceOf({})).toBe("linux");
        expect(referenceOf({ linux: "no", darwin: "no" })).toBe("win32");
        expect(() => referenceOf({ linux: "x", darwin: "x", win32: "x" })).toThrow(/runs nowhere/);
    });

    test("a platform that agrees adds nothing", () => {
        expect(after(linux, result({ "$ catter x": { stdout: "ok", exit: 0 } }), "win32")).toBe(
            linux,
        );
    });

    test("a platform that differs writes its own under the block", () => {
        expect(after(linux, result({ "$ catter x": { stdout: "bad", exit: 1 } }), "win32")).toBe(
            [
                '[test."$ catter x"]',
                'stdout = "ok"',
                "exit = 0",
                "",
                '[test."$ catter x".win32]',
                'stdout = "bad"',
                "exit = 1",
                "",
            ].join("\n"),
        );
    });

    test("its reason stays while it differs, and goes with it", () => {
        const flagged = [
            '[test."$ catter x"]',
            'stdout = "ok"',
            "exit = 0",
            "",
            '[test."$ catter x".win32]',
            'wrong = "splits arguments (catter#151)"',
            'stdout = "bad"',
            "exit = 1",
            "",
        ].join("\n");
        expect(
            after(flagged, result({ "$ catter x": { stdout: "worse", exit: 1 } }), "win32"),
        ).toBe(flagged.replace('"bad"', '"worse"'));
        expect(after(flagged, result({ "$ catter x": { stdout: "ok", exit: 0 } }), "win32")).toBe(
            linux,
        );
    });

    test("the reference rewrites the block and keeps the other platforms'", () => {
        const text = [
            '[test."$ catter x"]',
            'wrong = "exits 0 (catter#116)"',
            'stdout = "ok"',
            "exit = 0",
            "",
            '[test."$ catter x".darwin]',
            'differs = "Apple clang"',
            'stdout = "mac"',
            "exit = 0",
            "",
        ].join("\n");
        expect(after(text, result({ "$ catter x": { stdout: "new", exit: 0 } }), "linux")).toBe(
            text.replace('"ok"', '"new"'),
        );
    });

    test("a test that does not run here keeps its blocks", () => {
        expect(after(linux, result(undefined), "darwin")).toBe(linux);
    });

    test("a platform compares only once the reference wrote the blocks", () => {
        expect(() => after("", result({ "$ catter x": { exit: 0 } }), "win32")).toThrow(
            /update it there first/,
        );
    });

    test("every platform shows the same blocks", () => {
        expect(() => after(linux, result({ "$ catter y": { exit: 0 } }), "win32")).toThrow(
            /same blocks/,
        );
    });
});

describe("checkOutput", () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "catter-output-"));
    const c = { id: "topic/sub/01_case", dir } as Case;
    const file = path.join(dir, "snap.toml");

    afterEach(() => {
        vi.unstubAllEnvs();
        fs.rmSync(file, { force: true });
    });

    test("a platform's own output needs a reason", () => {
        fs.writeFileSync(file, after(linux, result({ "$ catter x": { exit: 1 } }), "win32"));
        expect(() => {
            checkOutput(c, result({ "$ catter x": { exit: 1 } }), "win32");
        }).toThrow(/say why/);
    });

    test("a difference fails, an update accepts it", () => {
        fs.writeFileSync(file, linux);
        expect(() => {
            checkOutput(c, result({ "$ catter x": { exit: 2 } }), "linux");
        }).toThrow(/differs from snap.toml/);
        vi.stubEnv("UPDATE_SNAPSHOTS", "1");
        checkOutput(c, result({ "$ catter x": { exit: 2 } }), "linux");
        expect(fs.readFileSync(file, "utf8")).toBe('[test."$ catter x"]\nexit = 2\n');
    });
});

describe("excepted platforms", () => {
    test("hold no output of the test", () => {
        const text = [
            '[test."$ catter x"]',
            "exit = 0",
            "",
            '[test."$ catter x".win32]',
            'differs = "gone now"',
            "exit = 1",
            "",
        ].join("\n");
        const [only] = result({ "$ catter x": { exit: 0 } });
        expect(after(text, [{ ...only!, excepted: ["win32"] }], "linux")).toBe(
            '[test."$ catter x"]\nexit = 0\n',
        );
    });
});
