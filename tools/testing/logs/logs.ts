/// What tests read from catter's logs: never the narrative, only the error
/// lines a test did not declare, and the excerpt a failure prints. A
/// record's header is `[time] [level] [pid:…][tid:…]` (after the source
/// place in a debug build), its message on the lines that follow.

import * as fs from "node:fs";
import * as path from "node:path";

export interface LogRecord {
    file: string;
    level: string;
    message: string;
}

const HEADER =
    /^(?:\[[^\]]*\] )?\[\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+\] \[(\w+)\] \[pid:\d+\]\[tid:\d+\]\s*$/;

/// The records of every log under `home`/log, in file order.
export function readLogs(home: string): LogRecord[] {
    const dir = path.join(home, "log");
    if (!fs.existsSync(dir)) {
        return [];
    }
    const records: LogRecord[] = [];
    for (const name of fs.readdirSync(dir).sort()) {
        let current: { level: string; lines: string[] } | undefined;
        const flush = () => {
            if (current !== undefined) {
                records.push({
                    file: name,
                    level: current.level,
                    message: current.lines.join("\n").trim(),
                });
            }
        };
        for (const line of fs.readFileSync(path.join(dir, name), "utf8").split(/\r?\n/)) {
            const header = HEADER.exec(line);
            if (header !== null) {
                flush();
                current = { level: header[1] ?? "", lines: [] };
            } else {
                current?.lines.push(line);
            }
        }
        flush();
    }
    return records;
}

function isError(record: LogRecord): boolean {
    return record.level === "error" || record.level === "critical";
}

/// Error records no pattern of `expected` matches.
export function undeclaredErrors(
    records: readonly LogRecord[],
    expected: readonly RegExp[],
): LogRecord[] {
    return records.filter(
        (record) => isError(record) && !expected.some((pattern) => pattern.test(record.message)),
    );
}

/// The records a failure prints: warnings and above, the last `limit`.
export function failureExcerpt(records: readonly LogRecord[], limit = 60): string {
    return records
        .filter((record) => record.level === "warning" || isError(record))
        .slice(-limit)
        .map((record) => `${record.file}: [${record.level}] ${record.message}`)
        .join("\n");
}
