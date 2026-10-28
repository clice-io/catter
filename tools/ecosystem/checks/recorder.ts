/// The recorder check: the execs a recorder saw around the run against the
/// commands catter reported, and the compiles among them against catter's
/// database (ecosystem/README.md, "The recorder check").
///
/// catter's own processes (catter, catter-proxy, the wrapper's shims) and
/// the handler's are dropped from the recording, and each exec's parent is
/// its nearest ancestor left: catter's model of a command's parent. Under
/// the hook the two trees match one to one, by argv; under the wrapper
/// catter sees only the programs its shims stand in for, so each of its
/// commands is among the recorder's, under the nearest one it reported too,
/// and every compiler, linker or archiver the build itself started is among
/// its commands.

import * as fs from "node:fs";
import * as path from "node:path";
import { expandResponseFiles, isCompile, isSource, isWrapped } from "../command.ts";
import type { Difference, Places } from "../difference.ts";
import type { Backend } from "../project.ts";
import type { Image, Recording } from "../record/images.ts";
import type { Command, Session } from "../session.ts";

export interface Entry {
    directory: string;
    file: string;
    output?: string;
    arguments: string[];
}

export interface RecorderInput {
    recording: Recording;
    session: Session;
    entries: readonly Entry[];
    backend: Backend;
    /// The catter that ran, resolved.
    catter: string;
    /// The directories whose programs are catter's own: its own, its data
    /// directory (the shims).
    own: readonly string[];
    /// The handler's script, whose process and what it starts are dropped.
    handler: string;
    places: Places;
}

export interface RecorderStats {
    recorded: number;
    kept: number;
    commands: number;
    matched: number;
    compiles: number;
}

function resolved(file: string): string {
    try {
        return fs.realpathSync.native(file);
    } catch {
        return file;
    }
}

function under(file: string, dirs: readonly string[]): boolean {
    return dirs.some((dir) => file === dir || file.startsWith(dir + path.sep));
}

export function checkRecorder(input: RecorderInput): { differences: Difference[]; stats: RecorderStats } {
    const { recording, session, places } = input;
    const differences: Difference[] = [];
    const add = (kind: string, detail: string, repro?: string) => {
        differences.push({ check: "recorder", kind, detail: places.spell(detail), ...(repro === undefined ? {} : { repro }) });
    };
    const stats: RecorderStats = { recorded: 0, kept: 0, commands: session.commands.length, matched: 0, compiles: 0 };
    if (recording.lost > 0) {
        add("lost", `the recorder lost ${recording.lost} events: what it lacks proves nothing`);
    }

    const images = recording.images;
    const parentOf = (image: Image): Image | undefined => (image.parent === null ? undefined : images[image.parent]);
    const isCatter = (image: Image) => path.basename(image.exe) === path.basename(input.catter) && resolved(image.exe) === input.catter;
    const top = images.find((image) => isCatter(image) && !ancestors(image).some(isCatter));
    function ancestors(image: Image): Image[] {
        const chain: Image[] = [];
        for (let at = parentOf(image); at !== undefined; at = parentOf(at)) {
            chain.push(at);
        }
        return chain;
    }
    if (top === undefined) {
        add("no-catter", "the recording holds no exec of the catter that ran");
        return { differences, stats };
    }
    // An image's place under the run: below the top, and not the handler's.
    const inRun = new Map<number, boolean>();
    const runs = (image: Image): boolean => {
        const known = inRun.get(image.id);
        if (known !== undefined) {
            return known;
        }
        const parent = parentOf(image);
        const result =
            parent !== undefined &&
            !image.args.includes(input.handler) &&
            (parent.id === top.id || runs(parent));
        inRun.set(image.id, result);
        return result;
    };
    const ownDirs = input.own.map(resolved);
    const isOwn = (image: Image) =>
        ["catter", "catter-proxy"].includes(path.basename(image.exe)) || under(resolved(image.exe), ownDirs);
    const run = images.filter(runs);
    const kept = run.filter((image) => !isOwn(image));
    const keptIds = new Set(kept.map((image) => image.id));
    stats.recorded = run.length;
    stats.kept = kept.length;
    const keptParent = (image: Image): number | null => ancestors(image).find((each) => keptIds.has(each.id))?.id ?? null;

    const recChildren = new Map<number | null, Image[]>();
    for (const image of kept) {
        const parent = keptParent(image);
        recChildren.set(parent, [...(recChildren.get(parent) ?? []), image]);
    }
    const catChildren = new Map<number | null, Command[]>();
    for (const command of session.commands) {
        catChildren.set(command.parent, [...(catChildren.get(command.parent) ?? []), command]);
    }
    const describe = (args: readonly string[], cwd: string) => `${places.label(args)} [in ${cwd}]`;
    const subtree = (image: Image): number =>
        (recChildren.get(image.id) ?? []).reduce((sum, child) => sum + 1 + subtree(child), 0);

    // Which recorded image each of catter's commands is.
    const imageOf = new Map<number, Image>();
    if (input.backend === "wrapper") {
        const free = new Map<string, Image[]>();
        for (const image of kept) {
            const key = JSON.stringify([image.args, image.cwd]);
            free.set(key, [...(free.get(key) ?? []), image]);
        }
        for (const command of session.commands) {
            const image = free.get(JSON.stringify([command.args, command.cwd]))?.shift();
            if (image === undefined) {
                add("extra", `catter reported ${describe(command.args, command.cwd)}, which the recorder never saw`);
            } else {
                imageOf.set(command.id, image);
            }
        }
        const commandOf = new Map([...imageOf].map(([id, image]) => [image.id, id]));
        for (const command of session.commands) {
            const image = imageOf.get(command.id);
            if (image === undefined) {
                continue;
            }
            const nearest = ancestors(image).find((each) => commandOf.has(each.id));
            const expected = nearest === undefined ? null : (commandOf.get(nearest.id) ?? null);
            if (expected !== command.parent) {
                add("parent", `catter gave ${describe(command.args, command.cwd)} another parent than the recorder's nearest command`);
            }
        }
        for (const image of kept) {
            const [program] = image.args;
            if (
                commandOf.has(image.id) ||
                program === undefined ||
                !(isWrapped(program) || isWrapped(image.exe)) ||
                ancestors(image).some((each) => commandOf.has(each.id))
            ) {
                continue;
            }
            add("missed", `catter missed ${describe(image.args, image.cwd)}`);
        }
    } else {
        const match = (catParent: number | null, recParent: number | null) => {
            const free = [...(recChildren.get(recParent) ?? [])];
            for (const command of catChildren.get(catParent) ?? []) {
                const same = free.filter((image) => image.args.length === command.args.length && image.args.every((arg, i) => arg === command.args[i]));
                const pick = same.find((image) => image.cwd === command.cwd) ?? same[0];
                if (pick === undefined) {
                    add("extra", `catter reported ${describe(command.args, command.cwd)}, which the recorder never saw there`);
                    continue;
                }
                free.splice(free.indexOf(pick), 1);
                imageOf.set(command.id, pick);
                if (pick.cwd !== command.cwd) {
                    add("cwd", `catter gave ${describe(command.args, command.cwd)} another directory than ${pick.cwd}`);
                }
                match(command.id, pick.id);
            }
            for (const image of free) {
                const below = subtree(image);
                add("missed", `catter missed ${describe(image.args, image.cwd)}${below > 0 ? ` and the ${below} commands it started` : ""}`);
            }
        };
        match(null, null);
    }
    stats.matched = imageOf.size;

    // The compiles the recorder saw against catter's database: an entry for
    // each source that is still there, each entry a recorded compile's.
    const recorded = new Map<string, { count: number; image: Image }>();
    for (const image of kept) {
        const args = expandResponseFiles(image.args, image.cwd);
        if (!isCompile(args)) {
            continue;
        }
        stats.compiles += 1;
        const sources = args.slice(1).filter((arg) => isSource(arg, image.cwd));
        const rest = args.filter((arg, i) => i === 0 || !sources.includes(arg));
        for (const source of sources) {
            const key = JSON.stringify([rest, image.cwd, path.resolve(image.cwd, source)]);
            const slot = recorded.get(key);
            recorded.set(key, { count: (slot?.count ?? 0) + 1, image });
        }
    }
    for (const entry of input.entries) {
        const args = expandResponseFiles(entry.arguments, entry.directory);
        const rest = args.filter((arg, i) => i === 0 || !isSource(arg, entry.directory));
        const key = JSON.stringify([rest, entry.directory, path.resolve(entry.directory, entry.file)]);
        const slot = recorded.get(key);
        if (slot === undefined || slot.count === 0) {
            add("entry-without-compile", `the entry of ${entry.file} is no compile the recorder saw: ${places.label(entry.arguments)} [in ${entry.directory}]`);
        } else {
            slot.count -= 1;
        }
    }
    for (const [key, slot] of recorded) {
        if (slot.count > 0) {
            const [, cwd, source] = JSON.parse(key) as [string[], string, string];
            add("no-entry", `no entry of ${source} for ${describe(slot.image.args, cwd)}`);
        }
    }
    return { differences, stats };
}
