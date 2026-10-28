/// A difference a check found between what catter answered and a reference,
/// and the known differences of a project it may be one of.

import type { Backend, Check, Known, Platform } from "./project.ts";

export interface Difference {
    check: Check;
    kind: string;
    /// One line, the places of the run spelled as placeholders, so that a
    /// known difference's pattern reads the same on every machine.
    detail: string;
    /// A command reproducing it, from the build's tree.
    repro?: string;
}

export interface Where {
    build: string;
    backend: Backend;
    platform: Platform;
}

export function isKnown(known: Known, difference: Difference, where: Where): boolean {
    return (
        known.check === difference.check &&
        known.kind === difference.kind &&
        (known.build === undefined || known.build === where.build) &&
        (known.backend === undefined || known.backend === where.backend) &&
        (known.platform === undefined || known.platform === where.platform) &&
        known.detail.test(difference.detail)
    );
}

/// The places of a run a detail names by placeholder, longest first.
export class Places {
    private readonly places: [string, string][];

    constructor(places: Record<string, string>) {
        this.places = Object.entries(places)
            .map(([name, dir]): [string, string] => [dir.replace(/[\\/]+$/, ""), `<${name}>`])
            .sort((a, b) => b[0].length - a[0].length);
    }

    spell(text: string): string {
        let out = text;
        for (const [dir, name] of this.places) {
            out = out.replaceAll(dir, name);
        }
        return out;
    }
}

const SHELL_SAFE = /^[\w./:=@%+,-]+$/;

export function quote(word: string): string {
    return SHELL_SAFE.test(word) ? word : `'${word.replaceAll("'", `'\\''`)}'`;
}

/// A command line for a detail: at most `width` characters.
export function label(args: readonly string[], width = 240): string {
    const line = args.map(quote).join(" ");
    return line.length > width ? `${line.slice(0, width - 1)}…` : line;
}
