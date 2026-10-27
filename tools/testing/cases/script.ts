/// What a case's test.ts registers: `test` scenarios, each run once on a
/// fresh copy of the case. A script is imported once and its registrations
/// serve every case that inherits it.

import { pathToFileURL } from "node:url";
import type { TestContext } from "./context.ts";
import type { Need } from "./needs.ts";
import type { Platform } from "./output.ts";

/// Platforms a test has no meaning on, each with the reason: elsewhere it
/// runs, there it is listed as not applicable.
export type Except = Partial<Record<Platform, string>>;

export interface TestOptions {
    except?: Except;
    /// What the test needs of the machine beside catter.
    needs?: Need[];
    /// Error-level log lines the test provokes, as regular expressions.
    errors?: string[];
}

export interface Registered {
    name: string;
    options: TestOptions;
    fn: (t: TestContext) => Promise<void>;
}

let sink: Registered[] | null = null;

function register(entry: Registered): void {
    if (sink === null) {
        throw new Error("test() belongs at the top level of a case's test.ts");
    }
    if (sink.some((other) => other.name === entry.name)) {
        throw new Error(`two tests named '${entry.name}'`);
    }
    sink.push(entry);
}

interface Register {
    (fn: (t: TestContext) => Promise<void>): void;
    (nameOrOptions: string | TestOptions, fn: (t: TestContext) => Promise<void>): void;
    (name: string, options: TestOptions, fn: (t: TestContext) => Promise<void>): void;
}

/// A scenario: commands and the disk, run once.
export const test: Register = (...args: unknown[]) => {
    const name = typeof args[0] === "string" ? (args.shift() as string) : "";
    const options = typeof args[0] === "object" ? (args.shift() as TestOptions) : {};
    register({
        name,
        options,
        fn: args[0] as (t: TestContext) => Promise<void>,
    });
};

/// A test's name in the output: its own, or `test` when it has none.
export function nameOf(registered: Registered): string {
    return registered.name === "" ? "test" : registered.name;
}

const loaded = new Map<string, Registered[]>();

/// The tests `file` registers.
export async function loadScript(file: string): Promise<Registered[]> {
    const cached = loaded.get(file);
    if (cached !== undefined) {
        return cached;
    }
    sink = [];
    try {
        await import(pathToFileURL(file).href);
        const registered = sink;
        if (registered.length === 0) {
            throw new Error(`${file} registers no test`);
        }
        if (registered.length > 1 && registered.some((entry) => entry.name === "")) {
            throw new Error(`${file}: an unnamed test must be the only one`);
        }
        loaded.set(file, registered);
        return registered;
    } finally {
        sink = null;
    }
}
