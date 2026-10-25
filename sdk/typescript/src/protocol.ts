// Generated from sdk/protocol.schema.json by sdk/generate.py: do not edit.
// The messages catter and a handler exchange.

/// Bytes that are not valid UTF-8, in base64.
export interface Base64 {
    base64: string;
}

/// Bytes from the system: a string when they are valid UTF-8.
export type OsString = string | Base64;

export interface Build {
    command: OsString[];
    cwd: OsString;
}

export interface InitializeParams {
    version: number;
    session: string;
    mode: string;
    build: Build;
}

export interface Events {
    command: boolean;
    error: boolean;
    exit: boolean;
}

export interface InitializeResult {
    decides: boolean;
    events: Events;
    env: boolean;
}

export interface Command {
    id: number;
    parent?: number | null;
    cwd: OsString;
    executable: OsString;
    args: OsString[];
    env?: OsString[] | null;
}

export interface CaptureError {
    id: number;
    parent?: number | null;
    message: string;
}

export interface Exit {
    id: number;
    code: number;
    duration: number;
}

export interface Finish {
    code: number;
}

export interface Skip {
    action: "skip";
}

export interface Drop {
    action: "drop";
}

export interface Abort {
    action: "abort";
    reason: string;
}

export interface Modify {
    action: "modify";
    args: OsString[];
    executable?: OsString | null;
    cwd?: OsString | null;
    env?: OsString[] | null;
}

export type Action = Skip | Drop | Abort | Modify;
