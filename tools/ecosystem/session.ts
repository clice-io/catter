/// catter's side of a run: the events handler.ts wrote, and the replay file
/// of them that `catter targets --from` reads.

import * as fs from "node:fs";
import type { Analysis } from "@catter/sdk";

export interface Command {
    id: number;
    parent: number | null;
    cwd: string;
    exe: string;
    args: string[];
    env: string[];
    analysis: Analysis | null;
    exit?: number;
}

export interface CaptureError {
    id: number;
    parent: number | null;
    message: string;
}

export interface Session {
    commands: Command[];
    errors: CaptureError[];
    /// How the build ended, as catter told the handler; none when the
    /// session never finished.
    finish?: number;
}

type Event =
    | ({ type: "command" } & Command)
    | ({ type: "error" } & CaptureError)
    | { type: "exit"; id: number; code: number }
    | { type: "finish"; code: number };

export function readSession(file: string): Session {
    const session: Session = { commands: [], errors: [] };
    if (!fs.existsSync(file)) {
        return session;
    }
    const byId = new Map<number, Command>();
    for (const line of fs.readFileSync(file, "utf8").split("\n")) {
        if (line === "") {
            continue;
        }
        const event = JSON.parse(line) as Event;
        switch (event.type) {
            case "command": {
                const command: Command = {
                    id: event.id,
                    parent: event.parent,
                    cwd: event.cwd,
                    exe: event.exe,
                    args: event.args,
                    env: event.env,
                    analysis: event.analysis,
                };
                session.commands.push(command);
                byId.set(command.id, command);
                break;
            }
            case "error":
                session.errors.push({ id: event.id, parent: event.parent, message: event.message });
                break;
            case "exit": {
                const command = byId.get(event.id);
                if (command !== undefined) {
                    command.exit = event.code;
                }
                break;
            }
            case "finish":
                session.finish = event.code;
                break;
        }
    }
    return session;
}

/// The session as a replay file (version 1), commands and errors in the
/// order catter reported them.
export function replayOf(session: Session): object {
    return {
        version: 1,
        events: [
            ...session.commands.map((command) => ({
                type: "command",
                id: command.id,
                parent: command.parent ?? 0,
                cwd: command.cwd,
                exe: command.exe,
                argv: command.args,
                ...(command.exit === undefined ? {} : { execution: { code: command.exit } }),
            })),
            ...session.errors.map((error) => ({
                type: "command",
                id: error.id,
                exe: "",
                argv: [],
                error: { message: error.message, parent: error.parent ?? 0 },
            })),
        ].sort((a, b) => a.id - b.id),
        finish: { code: session.finish ?? 0 },
    };
}
