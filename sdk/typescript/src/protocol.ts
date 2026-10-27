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
    decisions: boolean;
    build: Build;
}

export interface Events {
    command: boolean;
    error: boolean;
    exit: boolean;
}

export interface Pattern {
    kind: string;
    names?: string[] | null;
    phases?: string[] | null;
}

export interface InitializeResult {
    decides: boolean;
    events: Events;
    env: boolean;
    analysis: boolean;
    filter?: Pattern[] | null;
}

export interface Target {
    triple?: string | null;
    os?: string | null;
    objectFormat?: string | null;
    env?: string | null;
    artifactModel: string;
    source: string;
}

export interface Expansion {
    files: OsString[];
    args: OsString[];
}

export interface Argument {
    option: string;
    spelling: OsString;
    index: number;
    count: number;
    values: OsString[];
}

export interface Read {
    path: OsString;
    argument: number;
    index: number;
    role: string;
    language?: string | null;
}

export interface Write {
    path: OsString;
    reads: number[];
    origin: string;
}

export interface Diagnostic {
    code: string;
    message: string;
    path?: OsString | null;
    argument?: number | null;
    index?: number | null;
}

export interface Command {
    id: number;
    parent?: number | null;
    cwd: OsString;
    executable: OsString;
    args: OsString[];
    env?: OsString[] | null;
    analysis?: Analysis | null;
}

export interface CommandEdit {
    kind: string;
    flags: OsString[];
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

export interface Program {
    executable: OsString;
    args: OsString[];
    cwd?: OsString | null;
}

export interface AnalyzeParams {
    programs: Program[];
}

export interface AnalyzeResult {
    analyses: (Analysis | null)[];
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

export interface Edit {
    action: "edit";
    edits: CommandEdit[];
}

export type Action = Skip | Drop | Abort | Modify | Edit;

export interface CompilerAnalysis {
    kind: "compiler";
    compiler: string;
    dialect: string;
    phase: string;
    artifact: string;
    target: Target;
    expansion?: Expansion | null;
    arguments: Argument[];
    reads: Read[];
    writes: Write[];
    diagnostics: Diagnostic[];
}

export interface ArchiverAnalysis {
    kind: "archiver";
    operation: string;
    modifiers: string;
    thin: boolean;
    archive: OsString;
    members: OsString[];
    reads: OsString[];
    writes: OsString[];
}

export interface ToolAnalysis {
    kind: "tool";
    tool: string;
    arguments: Argument[];
}

export interface UnparsedAnalysis {
    kind: "unparsed";
    of: string;
    name: string;
    error: string;
    message: string;
}

export type Analysis = CompilerAnalysis | ArchiverAnalysis | ToolAnalysis | UnparsedAnalysis;
