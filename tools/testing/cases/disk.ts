/// The disk of a test's copy, `t.disk`: every change is a step, and what a
/// build leaves reads as values a test shows.

import * as fs from "node:fs";
import * as path from "node:path";
import type { StepRecord } from "../steps.ts";
import { mapText, type Table, type Value } from "../render/value.ts";
import { posix } from "./project.ts";

/// A change of a file's text: the unique place `replace` occurs replaced
/// `with` another text.
export interface Change {
    replace: string;
    with: string;
}

/// The fields of a compilation database entry, in the order shown; any
/// other field follows them.
const ENTRY_FIELDS = ["file", "directory", "output", "arguments", "command"];

function orderEntry(entry: Record<string, unknown>): Table {
    const keys = [
        ...ENTRY_FIELDS.filter((key) => key in entry),
        ...Object.keys(entry)
            .filter((key) => !ENTRY_FIELDS.includes(key))
            .sort(),
    ];
    return Object.fromEntries(keys.map((key) => [key, entry[key] as Value]));
}

export class Disk {
    private readonly root: string;
    private readonly steps: StepRecord;
    private readonly normalize: (text: string) => string;

    /// `normalize` reads a file's text as the output shows it.
    constructor(root: string, steps: StepRecord, normalize: (text: string) => string) {
        this.root = root;
        this.steps = steps;
        this.normalize = normalize;
    }

    private path(file: string): string {
        return path.join(this.root, file);
    }

    exists(file: string): boolean {
        return fs.existsSync(this.path(file));
    }

    read(file: string): string {
        return fs.readFileSync(this.path(file), "utf8");
    }

    /// Write one file, or several.
    write(file: string, text: string): void;
    write(files: Record<string, string>): void;
    write(fileOrFiles: string | Record<string, string>, text = ""): void {
        const files = typeof fileOrFiles === "string" ? { [fileOrFiles]: text } : fileOrFiles;
        const names = Object.keys(files);
        this.steps.note(
            names.length === 1
                ? `disk: write ${names[0] ?? ""}`
                : `disk: write ${String(names.length)} files`,
        );
        for (const [file, content] of Object.entries(files)) {
            const target = this.path(file);
            fs.mkdirSync(path.dirname(target), { recursive: true });
            fs.writeFileSync(target, content);
        }
    }

    edit(file: string, ...changes: Change[]): void {
        this.steps.note(
            `disk: edit ${file}: ${changes.map((change) => `replace ${JSON.stringify(change.replace)} with ${JSON.stringify(change.with)}`).join("; ")}`,
        );
        let text = this.read(file);
        for (const change of changes) {
            const at = text.indexOf(change.replace);
            if (at < 0 || text.includes(change.replace, at + 1)) {
                throw new Error(
                    `${file}: ${JSON.stringify(change.replace)} is not in it exactly once`,
                );
            }
            text = text.slice(0, at) + change.with + text.slice(at + change.replace.length);
        }
        fs.writeFileSync(this.path(file), text);
    }

    /// Remove a file or a directory with what it holds.
    rm(file: string): void {
        this.steps.note(`disk: remove ${file}`);
        fs.rmSync(this.path(file), { recursive: true });
    }

    /// The files matching `pattern`, copy-relative and sorted.
    list(pattern: string): string[] {
        return fs
            .globSync(pattern, { cwd: this.root })
            .map(posix)
            .filter((file) => fs.statSync(this.path(file)).isFile())
            .sort();
    }

    /// A compilation database as a test shows it: its entries normalized and
    /// sorted by file, output and command, each with its fields in a fixed
    /// order; `absent` when there is no such file, the text itself when it is
    /// no JSON array.
    cdb(file = "compile_commands.json"): Table {
        if (!this.exists(file)) {
            return { absent: true };
        }
        const text = this.read(file);
        let entries: unknown;
        try {
            entries = JSON.parse(text);
        } catch {
            return { text };
        }
        if (!Array.isArray(entries)) {
            return { text };
        }
        const key = (entry: Table): string =>
            JSON.stringify([
                entry["file"],
                entry["output"],
                entry["arguments"] ?? entry["command"],
            ]);
        const sorted = (entries as Record<string, unknown>[])
            .map((entry) => mapText(orderEntry(entry), this.normalize) as Table)
            .map((entry) => ({ entry, key: key(entry) }))
            .sort((a, b) => (a.key < b.key ? -1 : a.key > b.key ? 1 : 0))
            .map(({ entry }) => entry);
        return { entries: sorted };
    }
}
