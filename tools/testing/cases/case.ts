/// The cases under tests/: `<topic>/<subtopic>/NN_name/`, each a small
/// project with what to check. Read strictly: a stray file or a malformed
/// name is an error, never a silent default.

import * as fs from "node:fs";
import * as path from "node:path";
import { posix } from "./project.ts";
import { SUITES, TOPICS } from "./topics.ts";

export interface Case {
    /// `<topic>/<subtopic>/<name>`.
    id: string;
    topic: string;
    subtopic: string;
    name: string;
    dir: string;
    /// The script the case runs: its own test.ts, its subtopic's or its
    /// topic's.
    script: string;
}

const CASE = /^\d\d_[a-z0-9]+(_[a-z0-9]+)*$/;
const NAME = /^[a-z0-9]+(_[a-z0-9]+)*$/;

function fail(where: string, message: string): never {
    throw new Error(`${where}: ${message}`);
}

function subdirs(dir: string, where: string, allowed: readonly string[]): string[] {
    const out: string[] = [];
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        if (entry.isDirectory()) {
            out.push(entry.name);
        } else if (!allowed.includes(entry.name)) {
            fail(
                `${where}/${entry.name}`,
                `only ${allowed.join(" and ")} and directories belong here`,
            );
        }
    }
    return out.sort();
}

function readCase(
    root: string,
    topic: string,
    subtopic: string,
    name: string,
    scripts: readonly string[],
): Case {
    const dir = path.join(root, topic, subtopic, name);
    const id = `${topic}/${subtopic}/${name}`;
    const own = path.join(dir, "test.ts");
    const script = fs.existsSync(own) ? own : scripts.at(-1);
    if (script === undefined) {
        fail(id, "the case needs a test.ts, here or above");
    }
    return { id, topic, subtopic, name, dir, script };
}

/// Every case under `root`, in path order.
export function loadCases(root: string): Case[] {
    const cases: Case[] = [];
    const outerScripts: string[] = [];
    const scriptIn = (dir: string): string[] =>
        fs.existsSync(path.join(dir, "test.ts")) ? [path.join(dir, "test.ts")] : [];
    for (const topic of fs.readdirSync(root, { withFileTypes: true })) {
        if (!topic.isDirectory() || SUITES.has(topic.name)) {
            continue;
        }
        const topicDir = path.join(root, topic.name);
        if (!Object.hasOwn(TOPICS, topic.name)) {
            fail(topic.name, "not a topic: topics.ts lists them, as tests/README.md does");
        }
        for (const subtopic of subdirs(topicDir, topic.name, ["test.ts"])) {
            const subDir = path.join(topicDir, subtopic);
            const where = `${topic.name}/${subtopic}`;
            if (!NAME.test(subtopic) || subtopic === topic.name) {
                fail(
                    where,
                    "a subtopic names an aspect of its topic in lowercase words joined by _",
                );
            }
            const scripts = [...scriptIn(topicDir), ...scriptIn(subDir)];
            outerScripts.push(...scripts);
            for (const name of subdirs(subDir, where, ["test.ts"])) {
                if (!CASE.test(name)) {
                    fail(
                        `${where}/${name}`,
                        "a case is named NN_words: two digits, lowercase words joined by _",
                    );
                }
                cases.push(readCase(root, topic.name, subtopic, name, scripts));
            }
        }
    }
    const run = new Set(cases.map((c) => c.script));
    for (const script of new Set(outerScripts)) {
        if (!run.has(script)) {
            fail(
                posix(path.relative(root, script)),
                "no case runs this script: every case below has its own",
            );
        }
    }
    return cases.sort((a, b) => (a.id < b.id ? -1 : 1));
}

/// Shard `index` of `count` (`<index>/<count>`, 1-based) of `cases`: whole
/// subtopics, dealt largest first to the shard with the fewest cases so far.
export function shard(cases: readonly Case[], spec: string): Case[] {
    const match = /^(\d+)\/(\d+)$/.exec(spec);
    const index = Number(match?.[1]);
    const count = Number(match?.[2]);
    if (match === null || index < 1 || index > count) {
        fail("CATTER_CASES_SHARD", `'${spec}' is not <index>/<count>, counted from 1`);
    }
    const subtopics = new Map<string, Case[]>();
    for (const c of cases) {
        const key = `${c.topic}/${c.subtopic}`;
        subtopics.set(key, [...(subtopics.get(key) ?? []), c]);
    }
    const loads = Array.from({ length: count }, () => 0);
    const mine: Case[] = [];
    const largest = [...subtopics].sort(
        ([a, x], [b, y]) => y.length - x.length || (a < b ? -1 : 1),
    );
    for (const [, group] of largest) {
        const target = loads.indexOf(Math.min(...loads));
        loads[target] = (loads[target] ?? 0) + group.length;
        if (target === index - 1) {
            mine.push(...group);
        }
    }
    return mine.sort((a, b) => (a.id < b.id ? -1 : 1));
}
