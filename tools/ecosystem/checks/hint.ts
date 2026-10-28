/// The hint check: catter's database against the one the build system
/// writes itself (CMake's export, Meson's), which can be wrong too: a
/// difference is a lead, and one the build system is wrong about is known
/// with its reason.

import * as fs from "node:fs";
import * as path from "node:path";
import { expandResponseFiles, shellSplit } from "../command.ts";
import { label, type Difference, type Places } from "../difference.ts";
import type { Entry } from "./recorder.ts";

interface RawEntry {
    directory: string;
    file: string;
    output?: string;
    arguments?: string[];
    command?: string;
}

/// A database's entries, each with its arguments: a `command` split as a
/// shell splits it.
export function readDatabase(file: string): Entry[] {
    const raw = JSON.parse(fs.readFileSync(file, "utf8")) as RawEntry[];
    return raw.map((entry) => ({
        directory: entry.directory,
        file: entry.file,
        ...(entry.output === undefined ? {} : { output: entry.output }),
        arguments: entry.arguments ?? shellSplit(entry.command ?? ""),
    }));
}

function keyOf(entry: Entry): string {
    const output = entry.output === undefined ? "" : path.resolve(entry.directory, entry.output);
    return JSON.stringify([path.resolve(entry.directory), path.resolve(entry.directory, entry.file), output]);
}

/// Where two argument lists part, for a detail: the arguments around the
/// first one they differ at.
function parting(a: readonly string[], b: readonly string[]): string {
    let i = 0;
    while (i < a.length && i < b.length && a[i] === b[i]) {
        i += 1;
    }
    const around = (args: readonly string[]) => label(args.slice(Math.max(0, i - 1), i + 3), 120);
    return `at argument ${i}: catter has ${around(a)}, the build's ${around(b)}`;
}

export function checkHint(input: { entries: readonly Entry[]; hint: string | undefined; places: Places }): Difference[] {
    const differences: Difference[] = [];
    const add = (kind: string, detail: string) => {
        differences.push({ check: "hint", kind, detail: input.places.spell(detail) });
    };
    if (input.hint === undefined) {
        return differences;
    }
    if (!fs.existsSync(input.hint)) {
        add("no-hint", `the build wrote no ${input.hint}`);
        return differences;
    }
    const theirs = new Map<string, Entry[]>();
    for (const entry of readDatabase(input.hint)) {
        theirs.set(keyOf(entry), [...(theirs.get(keyOf(entry)) ?? []), entry]);
    }
    for (const entry of input.entries) {
        const same = theirs.get(keyOf(entry));
        const other = same?.shift();
        const file = path.resolve(entry.directory, entry.file);
        if (other === undefined) {
            add("catter-only", `the build's database has no entry of ${file}${entry.output === undefined ? "" : ` to ${entry.output}`}`);
            continue;
        }
        const ours = expandResponseFiles(entry.arguments, entry.directory);
        const its = expandResponseFiles(other.arguments, other.directory);
        if (ours.length !== its.length || ours.some((arg, i) => arg !== its[i])) {
            add("arguments", `${file}: ${parting(ours, its)}`);
        }
    }
    for (const left of theirs.values()) {
        for (const entry of left) {
            add("build-only", `catter's database has no entry of ${path.resolve(entry.directory, entry.file)}${entry.output === undefined ? "" : ` to ${entry.output}`}`);
        }
    }
    return differences;
}
