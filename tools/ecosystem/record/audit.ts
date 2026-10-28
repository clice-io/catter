/// Linux's audit subsystem as the recorder: auditd logs every exec and
/// fork system-wide under the run's key, and the images come from the raw
/// log (ecosystem/README.md, "Recorders"). It needs auditd running and sudo;
/// it costs a build a tenth or so, and no ptrace, so a sanitized catter runs
/// under it.

import { randomBytes } from "node:crypto";
import { spawnSync } from "node:child_process";
import { ImageBuilder, type Recording } from "./images.ts";
import type { Recorder } from "./recorder.ts";

const LOG = "/var/log/audit/audit.log";

/// The system calls by name, as the log's enriched part spells them, and by
/// their x86-64 numbers where it does not.
const EXECS = new Set(["execve", "execveat", "59", "322"]);
const FORKS = new Set(["clone", "clone3", "fork", "vfork", "56", "435", "57", "58"]);

const RECORD = /^type=(\w+) msg=audit\(\d+\.\d+:(\d+)\): (.*)$/;
const FIELD = /([\w[\]]+)=("[^"]*"|\S+)/g;

/// A field's value: quoted text as it is, anything else hex-encoded bytes
/// (auditd encodes a value holding a space, a quote, a control or a
/// non-ASCII character), `(null)` none.
export function auditValue(raw: string): string | undefined {
    if (raw.startsWith('"')) {
        return raw.slice(1, -1);
    }
    if (raw === "(null)") {
        return undefined;
    }
    return /^(?:[0-9A-F]{2})+$/.test(raw) ? Buffer.from(raw, "hex").toString("utf8") : raw;
}

function fields(text: string): Map<string, string> {
    const map = new Map<string, string>();
    for (const match of text.matchAll(FIELD)) {
        const [, key = "", value = ""] = match;
        if (!map.has(key)) {
            map.set(key, value);
        }
    }
    return map;
}

interface Event {
    syscall?: Map<string, string>;
    execve: Map<string, string>;
    cwd?: string;
}

function argv(execve: Map<string, string>): string[] {
    const count = Number(execve.get("argc") ?? "0");
    const args: string[] = [];
    for (let i = 0; i < count; i += 1) {
        const whole = execve.get(`a${i}`);
        if (whole !== undefined) {
            args.push(auditValue(whole) ?? "");
            continue;
        }
        // A long argument comes in chunks, `a1[0]=… a1[1]=…`, each of them
        // hex-encoded or each quoted.
        let joined = "";
        let quoted = false;
        for (let k = 0; execve.has(`a${i}[${k}]`); k += 1) {
            const chunk = execve.get(`a${i}[${k}]`) ?? "";
            quoted = chunk.startsWith('"');
            joined += quoted ? chunk.slice(1, -1) : chunk;
        }
        args.push(quoted ? joined : (auditValue(joined) ?? ""));
    }
    return args;
}

/// The images of the execs and forks a raw audit log records under `key`,
/// in the order the log has them.
export function parseAudit(log: string, key: string): ImageBuilder {
    const events = new Map<string, Event>();
    for (const line of log.split("\n")) {
        // The enriched part, after a group separator, spells names for ids.
        const [raw = "", enriched = ""] = line.split("\x1d");
        const record = RECORD.exec(raw);
        if (record === null) {
            continue;
        }
        const [, type, serial = "", rest = ""] = record;
        let event = events.get(serial);
        if (event === undefined) {
            event = { execve: new Map() };
            events.set(serial, event);
        }
        if (type === "SYSCALL") {
            event.syscall = fields(`${rest} ${enriched}`);
        } else if (type === "EXECVE") {
            for (const [name, value] of fields(rest)) {
                event.execve.set(name, value);
            }
        } else if (type === "CWD") {
            event.cwd = auditValue(fields(rest).get("cwd") ?? '""') ?? "";
        }
    }
    const builder = new ImageBuilder();
    for (const event of events.values()) {
        const call = event.syscall;
        if (call?.get("key") !== `"${key}"` || call.get("success") !== "yes") {
            continue;
        }
        const name = call.get("SYSCALL") ?? call.get("syscall") ?? "";
        const pid = Number(call.get("pid"));
        if (FORKS.has(name)) {
            builder.fork(pid, Number(call.get("exit")));
        } else if (EXECS.has(name)) {
            builder.exec(
                pid,
                Number(call.get("ppid")),
                auditValue(call.get("exe") ?? '""') ?? "",
                argv(event.execve),
                event.cwd ?? "",
            );
        }
    }
    return builder;
}

function sudo(args: string[]): string {
    const result = spawnSync("sudo", ["-n", ...args], { encoding: "utf8", maxBuffer: 1 << 30 });
    if (result.status !== 0) {
        throw new Error(`sudo ${args.join(" ")}: ${result.error?.message ?? result.stderr.trim()}`);
    }
    return result.stdout;
}

function lostCount(): number {
    return Number(/^lost (\d+)/m.exec(sudo(["auditctl", "-s"]))?.[1] ?? "0");
}

export class AuditRecorder implements Recorder {
    readonly name = "auditd";
    private readonly key = `catter-ecosystem-${randomBytes(4).toString("hex")}`;
    private offset = 0;
    private lost = 0;

    wrap(command: string[]): string[] {
        return command;
    }

    start(): void {
        this.offset = Number(sudo(["stat", "-c", "%s", LOG]).trim());
        this.lost = lostCount();
        sudo(["auditctl", "-b", "65536", "-r", "0"]);
        sudo([
            "auditctl",
            "-a",
            "always,exit",
            "-F",
            "arch=b64",
            "-S",
            "execve,execveat",
            "-k",
            this.key,
        ]);
        sudo([
            "auditctl",
            "-a",
            "always,exit",
            "-F",
            "arch=b64",
            "-S",
            "clone,clone3,fork,vfork",
            "-F",
            "success=1",
            "-k",
            this.key,
        ]);
    }

    /// The log read once it holds the exec of a marker run while the rules
    /// are still on (auditd writes what the kernel hands it a little later),
    /// then the rules off.
    async stop(): Promise<Recording> {
        const marker = `catter-ecosystem-marker-${randomBytes(4).toString("hex")}`;
        let log = "";
        for (let attempt = 0; ; attempt += 1) {
            spawnSync("/bin/true", [marker]);
            log = sudo(["tail", "-c", `+${this.offset + 1}`, LOG]);
            if (
                log.includes(Buffer.from(marker).toString("hex").toUpperCase()) ||
                log.includes(`"${marker}"`)
            ) {
                break;
            }
            if (attempt === 60) {
                throw new Error(`auditd never logged the marker exec: is it running, with ${LOG}?`);
            }
            await new Promise((resolve) => {
                // No event: auditd says nothing when it has written a record.
                setTimeout(resolve, 1000);
            });
        }
        sudo(["auditctl", "-D", "-k", this.key]);
        const size = Number(sudo(["stat", "-c", "%s", LOG]).trim());
        if (size < this.offset) {
            throw new Error(`${LOG} rotated during the build: raise max_log_file in auditd.conf`);
        }
        return { images: parseAudit(log, this.key).build(), lost: lostCount() - this.lost };
    }
}
