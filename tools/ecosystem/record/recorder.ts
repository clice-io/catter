/// A recorder of every exec around a run of catter, independent of catter:
/// what catter reports is compared with what it saw.

import type { Recording } from "./images.ts";

export const RECORDERS = ["auditd", "strace", "none"] as const;
export type RecorderName = (typeof RECORDERS)[number];

export interface Recorder {
    readonly name: RecorderName;
    /// The command line catter runs under, for a recorder that starts it.
    wrap(command: string[]): string[];
    start(): void;
    stop(): Promise<Recording>;
}

/// No recorder, for a run whose recorder check is not wanted: a machine
/// with neither auditd nor strace.
export class NoRecorder implements Recorder {
    readonly name = "none";

    wrap(command: string[]): string[] {
        return command;
    }

    start(): void {
        // Nothing records.
    }

    stop(): Promise<Recording> {
        return Promise.resolve({ images: [], lost: 0 });
    }
}
