/// A project of ecosystem/: where its source comes from, the builds it is
/// built by, and the differences already known (ecosystem/README.md).

import * as fs from "node:fs";
import * as path from "node:path";
import { parse } from "smol-toml";
import { REPO_ROOT } from "../repo.ts";

export const ECOSYSTEM_DIR = path.join(REPO_ROOT, "ecosystem");

export const PLATFORMS = ["linux", "macos", "windows"] as const;
export type Platform = (typeof PLATFORMS)[number];

export const BACKENDS = ["hook", "wrapper", "eslogger"] as const;
export type Backend = (typeof BACKENDS)[number];

export const CHECKS = ["recorder", "artifacts", "syntax", "hint", "catter"] as const;
export type Check = (typeof CHECKS)[number];

export type Step = string[];

export interface Build {
    name: string;
    about: string;
    platforms: Platform[];
    backends: Backend[];
    /// Debian packages the build needs on Linux.
    packages: string[];
    /// A directory of the project's own in ecosystem/<project>, built in
    /// place of the pinned source.
    tree?: string;
    env: Record<string, string>;
    /// Steps outside the session, before configure: what the project needs
    /// that no build of it does (a toolchain, a virtual environment).
    prepare: Step[];
    /// Steps outside the session, but under the wrapper backend, whose
    /// shims must be on PATH when a build system resolves its programs.
    configure: Step[];
    /// Steps in the session.
    build: Step[];
    /// The tree-relative directories whose new files are the build's
    /// artifacts; one that is a symlink is followed.
    scan: string[];
    /// The tree-relative compilation database the build system writes.
    hint?: string;
}

export interface Known {
    issue?: string;
    why: string;
    build?: string;
    backend?: Backend;
    platform?: Platform;
    check: Check;
    kind: string;
    detail: RegExp;
}

export interface Project {
    name: string;
    about: string;
    repository?: string;
    commit?: string;
    archive?: { url: string; sha256: string };
    builds: Build[];
    known: Known[];
}

/// A job of a round: a project's build on one platform, each of its
/// backends in turn.
export interface Job {
    name: string;
    platform: Platform;
    packages: string;
    /// The name its artifacts take.
    artifact: string;
}

type Table = Record<string, unknown>;

function fail(where: string, message: string): never {
    throw new Error(`${where}: ${message}`);
}

function string(table: Table, key: string, where: string): string | undefined {
    const value = table[key];
    if (value === undefined) {
        return undefined;
    }
    if (typeof value !== "string" || value === "") {
        fail(where, `${key} must be a non-empty string`);
    }
    return value;
}

function required(table: Table, key: string, where: string): string {
    return string(table, key, where) ?? fail(where, `${key} is missing`);
}

function strings(value: unknown, key: string, where: string): string[] {
    if (!Array.isArray(value) || !value.every((each) => typeof each === "string")) {
        fail(where, `${key} must be a list of strings`);
    }
    return value;
}

function steps(table: Table, key: string, where: string): Step[] {
    const value = table[key] ?? [];
    if (!Array.isArray(value)) {
        fail(where, `${key} must be a list of commands`);
    }
    return value.map((step, index) => {
        const words = strings(step, `${key}[${index}]`, where);
        if (words.length === 0) {
            fail(where, `${key}[${index}] is empty`);
        }
        return words;
    });
}

function oneOf<T extends string>(
    value: string,
    choices: readonly T[],
    what: string,
    where: string,
): T {
    if (!(choices as readonly string[]).includes(value)) {
        fail(where, `${what} ${value} is none of ${choices.join(", ")}`);
    }
    return value as T;
}

function only(table: Table, keys: readonly string[], where: string): void {
    for (const key of Object.keys(table)) {
        if (!keys.includes(key)) {
            fail(where, `unknown key ${key}`);
        }
    }
}

function readBuild(name: string, table: Table, dir: string, where: string): Build {
    only(
        table,
        [
            "about",
            "platforms",
            "backends",
            "packages",
            "tree",
            "env",
            "prepare",
            "configure",
            "build",
            "scan",
            "hint",
        ],
        where,
    );
    const platforms = strings(table["platforms"] ?? ["linux"], "platforms", where).map((each) =>
        oneOf(each, PLATFORMS, "platform", where),
    );
    const backends = strings(table["backends"] ?? ["hook"], "backends", where).map((each) =>
        oneOf(each, BACKENDS, "backend", where),
    );
    if (backends.includes("eslogger") && platforms.some((each) => each !== "macos")) {
        fail(where, "eslogger is macOS's backend alone");
    }
    const env = table["env"] ?? {};
    if (
        typeof env !== "object" ||
        Array.isArray(env) ||
        !Object.values(env).every((value) => typeof value === "string")
    ) {
        fail(where, "env must be a table of strings");
    }
    const tree = string(table, "tree", where);
    if (
        tree !== undefined &&
        !fs.statSync(path.join(dir, tree), { throwIfNoEntry: false })?.isDirectory()
    ) {
        fail(where, `tree ${tree} is no directory of the project's`);
    }
    const build = steps(table, "build", where);
    if (build.length === 0) {
        fail(where, "build is missing");
    }
    const hint = string(table, "hint", where);
    return {
        name,
        about: required(table, "about", where),
        platforms,
        backends,
        packages: strings(table["packages"] ?? [], "packages", where),
        ...(tree === undefined ? {} : { tree }),
        env: env as Record<string, string>,
        prepare: steps(table, "prepare", where),
        configure: steps(table, "configure", where),
        build,
        scan: strings(table["scan"] ?? ["."], "scan", where),
        ...(hint === undefined ? {} : { hint }),
    };
}

function readKnown(table: Table, builds: readonly Build[], where: string): Known {
    only(table, ["issue", "why", "build", "backend", "platform", "check", "kind", "detail"], where);
    const build = string(table, "build", where);
    if (build !== undefined && !builds.some((each) => each.name === build)) {
        fail(where, `no build ${build}`);
    }
    const backend = string(table, "backend", where);
    const platform = string(table, "platform", where);
    const issue = string(table, "issue", where);
    return {
        ...(issue === undefined ? {} : { issue }),
        why: required(table, "why", where),
        ...(build === undefined ? {} : { build }),
        ...(backend === undefined ? {} : { backend: oneOf(backend, BACKENDS, "backend", where) }),
        ...(platform === undefined
            ? {}
            : { platform: oneOf(platform, PLATFORMS, "platform", where) }),
        check: oneOf(required(table, "check", where), CHECKS, "check", where),
        kind: required(table, "kind", where),
        detail: new RegExp(required(table, "detail", where)),
    };
}

export function readProject(name: string, dir = path.join(ECOSYSTEM_DIR, name)): Project {
    const where = `ecosystem/${name}/project.toml`;
    const table = parse(fs.readFileSync(path.join(dir, "project.toml"), "utf8")) as Table;
    only(table, ["about", "repository", "commit", "archive", "builds", "known"], where);
    const repository = string(table, "repository", where);
    const commit = string(table, "commit", where);
    if ((repository === undefined) !== (commit === undefined)) {
        fail(where, "repository and commit go together");
    }
    if (commit !== undefined && !/^[0-9a-f]{40}$/.test(commit)) {
        fail(where, "commit must be a full hash");
    }
    let archive: Project["archive"];
    if (table["archive"] !== undefined) {
        const entry = table["archive"] as Table;
        only(entry, ["url", "sha256"], `${where} [archive]`);
        archive = { url: required(entry, "url", where), sha256: required(entry, "sha256", where) };
    }
    if (repository !== undefined && archive !== undefined) {
        fail(where, "a project comes from a repository or an archive, not both");
    }
    const builds = Object.entries((table["builds"] ?? {}) as Record<string, Table>).map(
        ([build, entry]) => readBuild(build, entry, dir, `${where} [builds.${build}]`),
    );
    if (builds.length === 0) {
        fail(where, "no builds");
    }
    for (const build of builds) {
        if (build.tree === undefined && repository === undefined && archive === undefined) {
            fail(where, `build ${build.name} has no source: no repository, archive or tree`);
        }
    }
    const known = ((table["known"] ?? []) as Table[]).map((entry, index) =>
        readKnown(entry, builds, `${where} [[known]] ${index + 1}`),
    );
    return {
        name,
        about: required(table, "about", where),
        ...(repository === undefined ? {} : { repository }),
        ...(commit === undefined ? {} : { commit }),
        ...(archive === undefined ? {} : { archive }),
        builds,
        known,
    };
}

export function projectNames(): string[] {
    return fs
        .readdirSync(ECOSYSTEM_DIR, { withFileTypes: true })
        .filter(
            (entry) =>
                entry.isDirectory() &&
                fs.existsSync(path.join(ECOSYSTEM_DIR, entry.name, "project.toml")),
        )
        .map((entry) => entry.name)
        .sort();
}

export function currentPlatform(): Platform {
    switch (process.platform) {
        case "linux":
            return "linux";
        case "darwin":
            return "macos";
        case "win32":
            return "windows";
        default:
            throw new Error(`no ecosystem platform is ${process.platform}`);
    }
}

/// Every job of a round, project by project.
export function listJobs(): Job[] {
    return projectNames().flatMap((name) =>
        readProject(name).builds.flatMap((build) =>
            build.platforms.map((platform) => ({
                name: `${name}/${build.name}`,
                platform,
                packages: platform === "linux" ? build.packages.join(" ") : "",
                artifact: `${name}-${build.name}-${platform}`,
            })),
        ),
    );
}

/// The project and build a job's name names.
export function findBuild(job: string): { project: Project; build: Build } {
    const [name, buildName, ...rest] = job.split("/");
    if (name === undefined || buildName === undefined || rest.length > 0) {
        throw new Error(`a job is named <project>/<build>, not ${job}`);
    }
    if (!projectNames().includes(name)) {
        throw new Error(`no project ${name} in ecosystem/`);
    }
    const project = readProject(name);
    const build = project.builds.find((each) => each.name === buildName);
    if (build === undefined) {
        throw new Error(
            `${name} has no build ${buildName}: ${project.builds.map((each) => each.name).join(", ")}`,
        );
    }
    return { project, build };
}
