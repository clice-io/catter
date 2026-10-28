/// A project's source: its pinned commit fetched alone, or its pinned
/// archive downloaded and checked, once per directory of projects; each
/// build of it a copy of its own. A source that cannot be had fails its job
/// as the setup's failure, never catter's.

import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { ECOSYSTEM_DIR, type Build, type Project } from "./project.ts";

function run(command: string, args: readonly string[], cwd: string): void {
    const result = spawnSync(command, args, { cwd, stdio: ["ignore", "inherit", "inherit"] });
    if (result.status !== 0) {
        throw new Error(`${[command, ...args].join(" ")} in ${cwd}: ${result.error?.message ?? `exit ${result.status ?? result.signal}`}`);
    }
}

function fetchCommit(repository: string, commit: string, target: string): void {
    if (fs.existsSync(path.join(target, ".git"))) {
        const head = spawnSync("git", ["rev-parse", "HEAD"], { cwd: target, encoding: "utf8" }).stdout.trim();
        if (head === commit) {
            return;
        }
        throw new Error(`${target} holds ${head}, not the pinned ${commit}: remove it first`);
    }
    fs.mkdirSync(target, { recursive: true });
    run("git", ["init", "-q"], target);
    run("git", ["fetch", "-q", "--depth", "1", repository, commit], target);
    run("git", ["-c", "advice.detachedHead=false", "checkout", "-q", "FETCH_HEAD"], target);
}

/// The marker an unpacked archive leaves: the hash it was checked against.
const UNPACKED = ".ecosystem-archive";

async function fetchArchive(url: string, sha256: string, target: string): Promise<void> {
    const marker = path.join(target, UNPACKED);
    if (fs.existsSync(marker)) {
        if (fs.readFileSync(marker, "utf8") === sha256) {
            return;
        }
        throw new Error(`${target} holds another archive than ${url}: remove it first`);
    }
    const response = await fetch(url);
    if (!response.ok) {
        throw new Error(`${url}: ${response.status} ${response.statusText}`);
    }
    const bytes = Buffer.from(await response.arrayBuffer());
    const got = createHash("sha256").update(bytes).digest("hex");
    if (got !== sha256) {
        throw new Error(`${url} has the SHA-256 ${got}, not the pinned ${sha256}`);
    }
    fs.mkdirSync(target, { recursive: true });
    const file = path.join(os.tmpdir(), `${got}-${path.basename(new URL(url).pathname)}`);
    fs.writeFileSync(file, bytes);
    if (file.endsWith(".zip")) {
        run("unzip", ["-q", file, "-d", target], target);
    } else {
        run("tar", ["-xf", file, "-C", target], target);
    }
    fs.rmSync(file);
    const unpacked = fs.readdirSync(target, { withFileTypes: true });
    const [top] = unpacked;
    if (unpacked.length === 1 && top?.isDirectory() === true) {
        const inner = path.join(target, top.name);
        for (const name of fs.readdirSync(inner)) {
            fs.renameSync(path.join(inner, name), path.join(target, name));
        }
        fs.rmdirSync(inner);
    }
    fs.writeFileSync(marker, sha256);
}

/// The pristine source of `build`: the project's own directory of it, or
/// its pinned source fetched under `dir`.
export async function sourceOf(project: Project, build: Build, dir: string): Promise<string> {
    if (build.tree !== undefined) {
        return path.join(ECOSYSTEM_DIR, project.name, build.tree);
    }
    const target = path.join(dir, project.name, "source");
    if (project.repository !== undefined && project.commit !== undefined) {
        fetchCommit(project.repository, project.commit, target);
    } else if (project.archive !== undefined) {
        await fetchArchive(project.archive.url, project.archive.sha256, target);
    }
    return target;
}

/// A fresh copy of `source` at `tree`, symlinks kept as they are.
export function copyTree(source: string, tree: string): void {
    fs.rmSync(tree, { recursive: true, force: true });
    fs.mkdirSync(path.dirname(tree), { recursive: true });
    fs.cpSync(source, tree, { recursive: true, verbatimSymlinks: true, preserveTimestamps: true });
}
