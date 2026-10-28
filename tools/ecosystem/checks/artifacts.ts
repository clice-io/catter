/// The artifacts check: the files the build wrote, told apart by their
/// magic, against what catter says wrote them. Each object is the output of
/// exactly one entry of the database (or of a relocatable link); each
/// archive, shared library and executable is what a link or an archive
/// command of catter's analysis writes, the commands `catter targets` is
/// made of.

import * as fs from "node:fs";
import * as path from "node:path";
import { type Difference, type Places } from "../difference.ts";
import type { Session } from "../session.ts";
import type { Entry } from "./recorder.ts";

export type FileKind = "object" | "linked" | "archive";

/// What a file is, by its first bytes: an object (ELF relocatable, Mach-O
/// MH_OBJECT, LLVM bitcode, a COFF object), a linked program or library
/// (ELF executable or shared, Mach-O executable, dylib or bundle, PE), an
/// archive; undefined for anything else. COFF objects have no magic: only a
/// `.obj` or `.o` whose machine field is one of Windows' counts.
export function fileKind(head: Buffer, name: string): FileKind | undefined {
    if (head.length >= 18 && head.toString("latin1", 0, 4) === "\x7fELF") {
        const type = head[5] === 2 ? head.readUInt16BE(16) : head.readUInt16LE(16);
        return type === 1 ? "object" : type === 2 || type === 3 ? "linked" : undefined;
    }
    const ar = head.toString("latin1", 0, 8);
    if (ar === "!<arch>\n" || ar === "!<thin>\n") {
        return "archive";
    }
    if (head.length >= 16) {
        const magic = head.readUInt32LE(0);
        if (magic === 0xfeedfacf || magic === 0xfeedface) {
            const type = head.readUInt32LE(12);
            return type === 1 ? "object" : [2, 6, 8].includes(type) ? "linked" : undefined;
        }
    }
    if (head.toString("latin1", 0, 4) === "BC\xc0\xde" || (head.length >= 4 && head.readUInt32LE(0) === 0x0b17c0de)) {
        return "object";
    }
    if (head.toString("latin1", 0, 2) === "MZ") {
        return "linked";
    }
    if (/\.(?:obj|o)$/i.test(name) && head.length >= 20) {
        const machine = head.readUInt16LE(0);
        const optionalHeader = head.readUInt16LE(16);
        if ([0x14c, 0x8664, 0xaa64, 0x1c4, 0xa641].includes(machine) && optionalHeader === 0) {
            return "object";
        }
    }
    return undefined;
}

export type Snapshot = Map<string, number>;

/// Every file under the scanned directories with its modification time;
/// a scanned directory that is a symlink is followed, nothing below it is.
export function snapshot(tree: string, scan: readonly string[]): Snapshot {
    const files: Snapshot = new Map();
    const walk = (dir: string) => {
        let entries: fs.Dirent[];
        try {
            entries = fs.readdirSync(dir, { withFileTypes: true });
        } catch {
            return;
        }
        for (const entry of entries) {
            const file = path.join(dir, entry.name);
            if (entry.isDirectory() && entry.name !== ".git") {
                walk(file);
            } else if (entry.isFile()) {
                files.set(file, fs.statSync(file).mtimeMs);
            }
        }
    };
    for (const dir of scan) {
        const root = path.resolve(tree, dir);
        if (fs.existsSync(root)) {
            walk(fs.realpathSync.native(root));
        }
    }
    return files;
}

function head(file: string): Buffer {
    const fd = fs.openSync(file, "r");
    try {
        const buffer = Buffer.alloc(64);
        const read = fs.readSync(fd, buffer, 0, 64, 0);
        return buffer.subarray(0, read);
    } finally {
        fs.closeSync(fd);
    }
}

function real(file: string): string {
    try {
        return fs.realpathSync.native(file);
    } catch {
        return path.resolve(file);
    }
}

export interface ArtifactStats {
    objects: number;
    linked: number;
    archives: number;
}

export function checkArtifacts(input: {
    before: Snapshot;
    after: Snapshot;
    entries: readonly Entry[];
    session: Session;
    places: Places;
}): { differences: Difference[]; stats: ArtifactStats } {
    const differences: Difference[] = [];
    const add = (kind: string, detail: string) => {
        differences.push({ check: "artifacts", kind, detail: input.places.spell(detail) });
    };
    const outputs = new Map<string, number>();
    for (const entry of input.entries) {
        if (entry.output !== undefined) {
            const file = real(path.resolve(entry.directory, entry.output));
            outputs.set(file, (outputs.get(file) ?? 0) + 1);
        }
    }
    const linked = new Set<string>();
    const relocatable = new Set<string>();
    for (const command of input.session.commands) {
        const analysis = command.analysis;
        if (analysis?.kind === "compiler" && ["link", "archive", "relocatable-link"].includes(analysis.phase)) {
            for (const write of analysis.writes) {
                const file = real(path.resolve(command.cwd, typeof write.path === "string" ? write.path : ""));
                (analysis.phase === "relocatable-link" ? relocatable : linked).add(file);
            }
        } else if (analysis?.kind === "archiver") {
            for (const file of [analysis.archive, ...analysis.writes]) {
                linked.add(real(path.resolve(command.cwd, typeof file === "string" ? file : "")));
            }
        }
    }
    // A file linked in place of a written one (cargo's binaries, out of
    // deps/) is that file.
    const inode = (file: string) => {
        const stat = fs.statSync(file, { throwIfNoEntry: false });
        return stat === undefined ? "" : `${stat.dev}:${stat.ino}`;
    };
    const linkedInodes = new Set([...linked].map(inode).filter((each) => each !== ""));
    const stats: ArtifactStats = { objects: 0, linked: 0, archives: 0 };
    for (const [file, mtime] of [...input.after].sort(([a], [b]) => a.localeCompare(b))) {
        if (input.before.get(file) === mtime) {
            continue;
        }
        const kind = fileKind(head(file), file);
        if (kind === undefined) {
            continue;
        }
        if (kind === "object") {
            stats.objects += 1;
            const count = outputs.get(file) ?? 0;
            if (count === 0 && !relocatable.has(file)) {
                add("object-without-entry", `no entry has the output ${file}`);
            } else if (count > 1) {
                add("object-in-several-entries", `${count} entries have the output ${file}`);
            }
        } else {
            stats[kind === "archive" ? "archives" : "linked"] += 1;
            if (!linked.has(file) && !linkedInodes.has(inode(file))) {
                add(kind === "archive" ? "archive-without-command" : "linked-without-command", `no link or archive command of catter's writes ${file}`);
            }
        }
    }
    return { differences, stats };
}
