/// The copy a test runs on: a case's inputs, `*.in` files with `${WS}`
/// replaced by the copy's root, in a directory named past ASCII.

import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { REPO_ROOT } from "../../repo.ts";

export const CASES_DIR = path.join(REPO_ROOT, "tests");

/// Files of a case directory that are not its inputs.
export const RESERVED = new Set(["test.ts", "README.md", "snap.toml"]);

export function posix(file: string): string {
    return file.split(path.sep).join("/");
}

/// The input files of a case directory, relative and sorted, as the case
/// names them.
export function inputFiles(dir: string): string[] {
    return fs
        .readdirSync(dir, { recursive: true, encoding: "utf8" })
        .map(posix)
        .filter((rel) => !RESERVED.has(rel) && fs.statSync(path.join(dir, rel)).isFile())
        .sort();
}

export function copyName(rel: string): string {
    return rel.endsWith(".in") ? rel.slice(0, -".in".length) : rel;
}

/// The name a new directory of a test takes: past ASCII, as a home or
/// project directory often is, in characters of two, three and four UTF-8
/// bytes (the last a UTF-16 surrogate pair).
export const PAST_ASCII = "é中😀-";

/// A new directory under `base` (the system's temporary directory by
/// default) named `<prefix>é中😀-XXXXXX`, resolved: macOS's temporary
/// directory is a symlink, and catter reports resolved paths.
export function freshDir(prefix: string, base = os.tmpdir()): string {
    return fs.realpathSync.native(fs.mkdtempSync(path.join(base, `${prefix}${PAST_ASCII}`)));
}

/// Copy the inputs of `dir` to `root`: `*.in` with `${WS}` replaced, any
/// other file byte for byte, its mode kept. Returns the copy's file names.
export function materialize(dir: string, root: string): string[] {
    const workspace = posix(root);
    const files: string[] = [];
    for (const rel of inputFiles(dir)) {
        const name = copyName(rel);
        const source = path.join(dir, rel);
        const target = path.join(root, name);
        fs.mkdirSync(path.dirname(target), { recursive: true });
        if (rel.endsWith(".in")) {
            fs.writeFileSync(
                target,
                fs.readFileSync(source, "utf8").replaceAll("${WS}", workspace),
            );
        } else {
            fs.copyFileSync(source, target);
        }
        fs.chmodSync(target, fs.statSync(source).mode);
        files.push(name);
    }
    return files;
}
