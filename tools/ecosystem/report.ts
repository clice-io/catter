/// The summaries: a job's, from its results, and a round's, from every
/// job's. New differences come first, each with what reproduces it, then
/// the known ones with their issue or reason, then the known ones no run
/// showed.

import type { BackendResult, Found, JobResult } from "./job.ts";
import type { Job } from "./project.ts";

/// A job is red when it stopped, a backend could not be checked or found a
/// difference no known one explains.
export function isRed(result: JobResult): boolean {
    return (
        result.error !== undefined ||
        result.backends.some(
            (backend) =>
                backend.setup !== undefined ||
                backend.differences.some((each) => each.known === undefined),
        )
    );
}

function numbers(backend: BackendResult): string {
    const parts = [`catter exit ${backend.exit ?? "killed"}`, `${backend.entries} entries`];
    if (backend.recorded !== undefined) {
        const r = backend.recorded;
        parts.push(
            `${r.recorded} execs recorded, ${r.recorded - r.kept} of them catter's own; ${r.commands} commands of catter's, ${r.matched} of them matched; ${r.compiles} compiles recorded`,
        );
    }
    if (backend.artifacts !== undefined) {
        const a = backend.artifacts;
        parts.push(
            `${a.objects} objects, ${a.archives} archives, ${a.linked} programs and libraries`,
        );
    }
    if (backend.syntax !== undefined) {
        const s = backend.syntax;
        parts.push(
            `syntax ${s.passed}/${s.checked}${s.gone > 0 ? `, ${s.gone} sources gone` : ""}${s.skipped > 0 ? `, ${s.skipped} not GCC-like` : ""}`,
        );
    }
    return parts.join("; ");
}

function block(text: string): string {
    return text.includes("\n")
        ? `\n\n  \`\`\`\n${text.replace(/^/gm, "  ")}\n  \`\`\``
        : ` \`${text.replaceAll("`", "'")}\``;
}

function line(found: Found): string {
    const head = `- **${found.check}/${found.kind}**${block(found.detail)}`;
    const known =
        found.known === undefined
            ? ""
            : ` — ${found.known.issue === undefined ? "" : `${found.known.issue}: `}${found.known.why}`;
    const repro = found.repro === undefined ? "" : `\n  - reproduce: \`${found.repro}\``;
    return `${head}${known}${repro}`;
}

/// At most `limit` lines of a list, and how many more there were.
function capped(lines: readonly string[], limit = 100): string[] {
    return lines.length > limit
        ? [...lines.slice(0, limit), `- … and ${lines.length - limit} more, in results.json`]
        : [...lines];
}

export function jobReport(result: JobResult): string {
    const out = [`## ${isRed(result) ? "🔴" : "🟢"} ${result.job} (${result.platform})`, ""];
    if (result.error !== undefined) {
        out.push(`**The job stopped:** ${result.error}`, "");
    }
    for (const backend of result.backends) {
        out.push(`### ${backend.backend}, recorded by ${backend.recorder}`, "");
        if (backend.setup !== undefined) {
            out.push(
                `**Not checked:** ${backend.setup}`,
                "",
                `Reproduce: \`${backend.repro}\``,
                "",
            );
            continue;
        }
        out.push(numbers(backend), "");
        const fresh = backend.differences.filter((each) => each.known === undefined);
        const known = backend.differences.filter((each) => each.known !== undefined);
        if (fresh.length > 0) {
            out.push(
                `**${fresh.length} new**, the run reproduced by \`${backend.repro}\`:`,
                "",
                ...capped(fresh.map(line)),
                "",
            );
        }
        if (known.length > 0) {
            out.push(`${known.length} known:`, "", ...capped(known.map(line)), "");
        }
        if (fresh.length === 0 && known.length === 0) {
            out.push("No difference.", "");
        }
    }
    if (result.unseen.length > 0) {
        out.push(
            "Known differences no run showed (fixed, or hidden by another):",
            "",
            ...result.unseen.map(
                (each) =>
                    `- ${each.check}/${each.kind} \`${each.detail}\` — ${each.issue === undefined ? "" : `${each.issue}: `}${each.why}`,
            ),
            "",
        );
    }
    return out.join("\n");
}

/// The round's summary: a line per job of the round, one with no results
/// red, then every red job's new differences.
export function roundReport(results: readonly JobResult[], jobs: readonly Job[]): string {
    const out = [
        "# Ecosystem round",
        "",
        "| job | platform | backends | new | known |",
        "| --- | --- | --- | --- | --- |",
    ];
    for (const job of jobs) {
        if (!results.some((result) => result.job === job.name)) {
            out.push(`| 🔴 ${job.name} | ${job.platform} | | no results | |`);
        }
    }
    for (const result of results) {
        const fresh = result.backends.reduce(
            (sum, b) =>
                sum +
                (b.setup === undefined
                    ? b.differences.filter((d) => d.known === undefined).length
                    : 0),
            0,
        );
        const known = result.backends.reduce(
            (sum, b) => sum + b.differences.filter((d) => d.known !== undefined).length,
            0,
        );
        const setup = [
            ...(result.error === undefined ? [] : ["stopped"]),
            ...result.backends
                .filter((b) => b.setup !== undefined)
                .map((b) => `${b.backend} not checked`),
        ];
        out.push(
            `| ${isRed(result) ? "🔴" : "🟢"} ${result.job} | ${result.platform} | ${result.backends.map((b) => b.backend).join(", ")} | ${[fresh, ...setup].join(", ")} | ${known} |`,
        );
    }
    out.push("");
    for (const result of results.filter(isRed)) {
        out.push(`## ${result.job} (${result.platform})`, "");
        if (result.error !== undefined) {
            out.push(`The job stopped: ${result.error}`, "");
        }
        for (const backend of result.backends) {
            if (backend.setup !== undefined) {
                out.push(`- ${backend.backend}: not checked: ${backend.setup}`);
                continue;
            }
            const fresh = backend.differences.filter((each) => each.known === undefined);
            if (fresh.length > 0) {
                out.push(
                    `${backend.backend}: \`${backend.repro}\``,
                    "",
                    ...capped(fresh.map(line), 30),
                    "",
                );
            }
        }
    }
    return out.join("\n");
}
