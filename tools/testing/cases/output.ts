/// A case's output, snap.toml: per test, the blocks it showed in run order,
/// `[<test>.<label>]` each, a table: a command's transcript under its
/// command line, anything else under the label the test gave it.
///
/// One file serves every platform. A block holds the output of the test's
/// reference platform, the first of linux, darwin and win32 it runs on;
/// another platform whose output differs holds its own under the block,
/// `[<test>.<label>.win32]`, with the reason: `wrong = "why"` for a bug,
/// `differs = "why"` for what rightly differs there. A block known wrong on
/// its reference platform carries `wrong` itself. Every update keeps the
/// reasons while the block exists. Compared with snap.toml, rewritten under
/// UPDATE_SNAPSHOTS=1 with a summary of what moved.

import * as fs from "node:fs";
import * as path from "node:path";
import { createTwoFilesPatch } from "diff";
import { canonical, isTable, readToml, trimmed, writeToml, type Table } from "../render/value.ts";
import type { Case } from "./case.ts";

export const SNAPSHOT = "snap.toml";

export const PLATFORMS = ["linux", "darwin", "win32"] as const;
export type Platform = (typeof PLATFORMS)[number];

/// The reason a block's output is known wrong.
export const WRONG = "wrong";
/// The reason a platform's output rightly differs from the reference.
export const DIFFERS = "differs";
const REASONS = [WRONG, DIFFERS];
const SEPARATOR = " ▸ ";

export function isPlatform(key: string): key is Platform {
    return (PLATFORMS as readonly string[]).includes(key);
}

/// The platform the tests run on.
export function hostPlatform(): Platform {
    const platform = process.platform;
    if (!isPlatform(platform)) {
        throw new Error(`the cases do not run on ${platform}`);
    }
    return platform;
}

/// One test's blocks, in the order shown.
export class Output {
    readonly blocks = new Map<string, Table>();

    block(label: string, table: Table): void {
        if (this.blocks.has(label)) {
            throw new Error(`two blocks labeled '${label}'`);
        }
        this.set(label, table);
    }

    private set(label: string, table: Table): void {
        for (const key of Object.keys(table)) {
            if (REASONS.includes(key) || isPlatform(key)) {
                throw new Error(
                    `block '${label}': '${key}' is snap.toml's own key, which no output may use`,
                );
            }
        }
        this.blocks.set(label, table);
    }

    /// The place of a command's transcript, taken as the command starts so
    /// that commands run side by side keep the order they were started in:
    /// its command line, a command run again `#2`, and so on.
    reserve(label: string): string {
        let key = label;
        for (let count = 2; this.blocks.has(key); count += 1) {
            key = `${label} #${String(count)}`;
        }
        this.blocks.set(key, {});
        return key;
    }

    /// The transcript of the command `reserve` gave `key`.
    transcript(key: string, table: Table): void {
        this.set(key, table);
    }
}

export type Blocks = ReadonlyMap<string, Table>;

/// A block as snap.toml holds it: the reference platform's output, its
/// known-wrong reason among it, and each platform that differs.
export interface Block {
    own: Table;
    platforms: ReadonlyMap<Platform, Table>;
}

/// A case's blocks by test, in the file's order.
export type Snapshot = ReadonlyMap<string, ReadonlyMap<string, Block>>;

function without(table: Table, keys: readonly string[]): Table {
    return Object.fromEntries(Object.entries(table).filter(([key]) => !keys.includes(key)));
}

function reasonsOf(table: Table | undefined): Table {
    return table === undefined
        ? {}
        : Object.fromEntries(Object.entries(table).filter(([key]) => REASONS.includes(key)));
}

/// Whether two outputs read the same, their reasons left out.
function same(a: Table, b: Table): boolean {
    return canonical(trimmed(without(a, REASONS))) === canonical(trimmed(without(b, REASONS)));
}

export function readSnapshot(text: string): Snapshot {
    const out = new Map<string, Map<string, Block>>();
    for (const [test, blocks] of Object.entries(readToml(text))) {
        const read = new Map<string, Block>();
        for (const [label, table] of Object.entries(blocks as Record<string, Table>)) {
            const platforms = new Map<Platform, Table>();
            const own: Record<string, Table[string]> = {};
            for (const [key, value] of Object.entries(table)) {
                if (isPlatform(key) && isTable(value)) {
                    platforms.set(key, value);
                } else {
                    own[key] = value;
                }
            }
            read.set(label, { own, platforms });
        }
        out.set(test, read);
    }
    return out;
}

export function writeSnapshot(snapshot: Snapshot): string {
    return writeToml(
        [...snapshot].flatMap(([test, blocks]) =>
            [...blocks].map(([label, block]) => {
                const reasons = reasonsOf(block.own);
                const table: Table = {
                    ...reasons,
                    ...without(block.own, REASONS),
                    ...Object.fromEntries(
                        PLATFORMS.flatMap((platform) => {
                            const own = block.platforms.get(platform);
                            return own === undefined
                                ? []
                                : [[platform, { ...reasonsOf(own), ...without(own, REASONS) }]];
                        }),
                    ),
                };
                return { path: [test, label], table };
            }),
        ),
    );
}

/// What one test left on this platform.
export interface TestResult {
    name: string;
    /// The platform whose output the test's blocks hold.
    reference: Platform;
    /// Its blocks, or undefined where the test has no meaning.
    blocks: Blocks | undefined;
    /// The platforms the test has no meaning on, which hold no output of it.
    excepted: readonly Platform[];
}

/// `blocks` without the outputs of the platforms in `excepted`.
function dropPlatforms(
    blocks: ReadonlyMap<string, Block>,
    excepted: readonly Platform[],
): Map<string, Block> {
    return new Map(
        [...blocks].map(([label, block]) => [
            label,
            {
                own: block.own,
                platforms: new Map(
                    [...block.platforms].filter(([platform]) => !excepted.includes(platform)),
                ),
            },
        ]),
    );
}

/// The platform whose output a test's blocks hold: the first it runs on.
export function referenceOf(except: Partial<Record<Platform, string>>): Platform {
    const platform = PLATFORMS.find((each) => except[each] === undefined);
    if (platform === undefined) {
        throw new Error("a test excepted on every platform runs nowhere");
    }
    return platform;
}

/// The snapshot with what the tests left on `platform` in it: the
/// reference platform rewrites the blocks, keeping the other platforms'
/// own; another platform writes its own where it differs and drops it
/// where it agrees. A test that did not run here keeps its blocks.
export function merge(
    recorded: Snapshot,
    results: readonly TestResult[],
    platform: Platform,
    where: string,
): Snapshot {
    const out = new Map<string, Map<string, Block>>();
    for (const result of results) {
        const old = recorded.get(result.name) ?? new Map<string, Block>();
        if (result.blocks === undefined) {
            if (old.size > 0) {
                out.set(result.name, dropPlatforms(old, result.excepted));
            }
            continue;
        }
        const merged = new Map<string, Block>();
        if (platform === result.reference) {
            for (const [label, table] of result.blocks) {
                const before = old.get(label);
                const platforms = new Map(before?.platforms);
                platforms.delete(platform);
                merged.set(label, {
                    own: {
                        ...(before === undefined ? {} : reasonsOf(without(before.own, [DIFFERS]))),
                        ...table,
                    },
                    platforms,
                });
            }
        } else {
            const shown = [...result.blocks.keys()];
            const kept = [...old.keys()];
            if (kept.length === 0) {
                throw new Error(
                    `${where} > ${result.name}: no output of ${result.reference} yet, which ${platform} compares with: update it there first`,
                );
            }
            if (canonical(shown) !== canonical(kept)) {
                throw new Error(
                    `${where} > ${result.name}: ${platform} shows ${JSON.stringify(shown)}, ${result.reference} ${JSON.stringify(kept)}: a test shows the same blocks on every platform it runs on; one whose commands differ is a test per platform`,
                );
            }
            for (const [label, before] of old) {
                const table = result.blocks.get(label) ?? {};
                const platforms = new Map(before.platforms);
                if (same(before.own, table)) {
                    platforms.delete(platform);
                } else {
                    platforms.set(platform, {
                        ...reasonsOf(before.platforms.get(platform)),
                        ...table,
                    });
                }
                merged.set(label, { own: before.own, platforms });
            }
        }
        out.set(result.name, dropPlatforms(merged, result.excepted));
    }
    return out;
}

/// The blocks whose reasons are missing or out of place, as messages.
function misplacedReasons(snapshot: Snapshot): string[] {
    const out: string[] = [];
    for (const [test, blocks] of snapshot) {
        for (const [label, block] of blocks) {
            const key = `[${test}."${label}"]`;
            for (const reason of REASONS) {
                for (const table of [block.own, ...block.platforms.values()]) {
                    const value = table[reason];
                    if (value !== undefined && typeof value !== "string") {
                        out.push(`${key}: ${reason} is the reason, a string`);
                    }
                }
            }
            if (block.own[DIFFERS] !== undefined) {
                out.push(
                    `${key}: differs belongs to a platform's block, saying why it differs from this one`,
                );
            }
            for (const [platform, table] of block.platforms) {
                if (table[WRONG] === undefined && table[DIFFERS] === undefined) {
                    out.push(
                        `${key}: ${platform}'s output differs: say why, wrong = "…" for a bug, differs = "…" for what rightly differs`,
                    );
                }
            }
        }
    }
    return out;
}

interface Change {
    id: string;
    changed: string[];
    fixed: string[];
    added: string[];
    removed: string[];
    unexplained: string[];
}

/// Each output's text by `test ▸ label` or `test ▸ label ▸ platform`, and
/// whether it is known wrong.
function outputs(snapshot: Snapshot): Map<string, { text: string; wrong: boolean }> {
    const out = new Map<string, { text: string; wrong: boolean }>();
    for (const [test, blocks] of snapshot) {
        for (const [label, block] of blocks) {
            const key = `${test}${SEPARATOR}${label}`;
            out.set(key, {
                text: canonical(trimmed(without(block.own, REASONS))),
                wrong: block.own[WRONG] !== undefined,
            });
            for (const [platform, table] of block.platforms) {
                out.set(`${key}${SEPARATOR}${platform}`, {
                    text: canonical(trimmed(without(table, REASONS))),
                    wrong: table[WRONG] !== undefined,
                });
            }
        }
    }
    return out;
}

function recordChange(dir: string, c: Case, before: Snapshot, after: Snapshot): void {
    const old = outputs(before);
    const now = outputs(after);
    const change: Change = {
        id: c.id,
        changed: [],
        fixed: [],
        added: [],
        removed: [],
        unexplained: misplacedReasons(after),
    };
    for (const [key, output] of now) {
        const previous = old.get(key);
        if (previous === undefined) {
            change.added.push(key);
        } else if (previous.text !== output.text) {
            (previous.wrong ? change.fixed : change.changed).push(key);
        }
    }
    for (const [key, output] of old) {
        if (!now.has(key)) {
            change.removed.push(output.wrong ? `${key} (was known wrong)` : key);
        }
    }
    fs.mkdirSync(dir, { recursive: true });
    fs.appendFileSync(
        path.join(dir, `${String(process.pid)}.jsonl`),
        `${JSON.stringify(change)}\n`,
    );
}

/// What the update run changed, case by case: known-wrong outputs that
/// moved (likely fixed) apart from the rest (likely regressed), and the
/// platform outputs that still need a reason.
export function renderSummary(dir: string): string {
    if (!fs.existsSync(dir)) {
        return "";
    }
    const changes = fs
        .readdirSync(dir)
        .flatMap((name) => fs.readFileSync(path.join(dir, name), "utf8").split("\n"))
        .filter((line) => line !== "")
        .map((line) => JSON.parse(line) as Change)
        .filter(
            (change) =>
                change.changed.length +
                    change.fixed.length +
                    change.added.length +
                    change.removed.length +
                    change.unexplained.length >
                0,
        )
        .sort((a, b) => (a.id < b.id ? -1 : 1));
    if (changes.length === 0) {
        return "";
    }
    const regressed = changes.filter((change) => change.changed.length + change.removed.length > 0);
    const out = [
        `outputs changed in ${String(changes.length)} cases, ${String(regressed.length)} with endorsed output changed or gone`,
    ];
    for (const change of changes) {
        out.push(`  ${change.id}`);
        const kinds: [string, string[]][] = [
            ["changed", change.changed],
            ["removed", change.removed],
            ["changed, was known wrong", change.fixed],
            ["added", change.added],
            ["needs a reason", change.unexplained],
        ];
        for (const [kind, keys] of kinds) {
            if (keys.length > 0) {
                out.push(`    ${kind}: ${keys.join(", ")}`);
            }
        }
    }
    return out.join("\n");
}

function snapshotPath(c: Case): string {
    return path.join(c.dir, SNAPSHOT);
}

/// The case's snap.toml as it stands, its line ends LF as the framework
/// writes them, whatever a checkout made of them.
export function recorded(c: Case): string {
    const file = snapshotPath(c);
    return fs.existsSync(file) ? fs.readFileSync(file, "utf8").replaceAll("\r\n", "\n") : "";
}

/// Compare what the tests left on this platform with the case's snap.toml,
/// or rewrite it when updating.
export function checkOutput(
    c: Case,
    results: readonly TestResult[],
    platform: Platform = hostPlatform(),
): void {
    const file = snapshotPath(c);
    const before = recorded(c);
    const old = readSnapshot(before);
    const now = merge(old, results, platform, c.id);
    const text = writeSnapshot(now);
    if (process.env["UPDATE_SNAPSHOTS"] === "1") {
        if (before !== text) {
            if (text === "") {
                fs.rmSync(file);
            } else {
                fs.writeFileSync(file, text);
            }
        }
        const summary = process.env["CATTER_SNAPSHOT_SUMMARY_DIR"];
        if (summary !== undefined) {
            recordChange(summary, c, old, now);
        }
        return;
    }
    if (before !== text) {
        const patch = createTwoFilesPatch(
            SNAPSHOT,
            `this run on ${platform}`,
            before,
            text,
            "",
            "",
            { context: 3 },
        );
        throw new Error(
            `${c.id}: the output differs from ${SNAPSHOT} (UPDATE_SNAPSHOTS=1 accepts it)\n${patch}`,
        );
    }
    const misplaced = misplacedReasons(now);
    if (misplaced.length > 0) {
        throw new Error(`${c.id}: ${SNAPSHOT}\n  ${misplaced.join("\n  ")}`);
    }
}
