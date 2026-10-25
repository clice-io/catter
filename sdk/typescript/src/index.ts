/// Handlers for catter: a handler is told of the commands a build under
/// catter starts and, if it decides, answers each with what catter does with
/// it.
///
/// ```ts
/// import { serve } from "@catter/sdk";
///
/// await serve({
///     decides: true,
///     onCommand(command) {
///         return command.args.includes("-O0")
///             ? { action: "modify", args: [...command.args, "-g"] }
///             : undefined; // { action: "skip" }: run it as it is
///     },
/// });
/// ```
///
/// and `catter run --handler "node handler.ts" -- make` runs the build with it.
/// `listen()` serves catter's `--connect` instead.
///
/// A handler may ask for what catter makes of each command (`analysis`), be
/// told of only some commands (`filter`), edit a command's arguments
/// (`{ action: "edit" }`), and ask what catter makes of commands of its own
/// (`catter.analyze()`, through every callback's second argument).
///
/// Every callback is optional: a handler is told of the events it has a
/// callback for. Once the SDK is imported, what the program prints with
/// console goes to stderr: its stdout carries the protocol. An argument, a path or an environment entry that is not
/// UTF-8 comes as `{ base64 }`, which a command handed back keeps.

import { Console } from "node:console";
import * as net from "node:net";
import type { Readable, Writable } from "node:stream";
import {
    createMessageConnection,
    ResponseError,
    StreamMessageReader,
    StreamMessageWriter,
} from "vscode-jsonrpc/node.js";

import type {
    Action,
    Analysis,
    AnalyzeParams,
    AnalyzeResult,
    CaptureError,
    Command,
    Exit,
    Finish,
    InitializeParams,
    InitializeResult,
    Pattern,
    Program,
} from "./protocol.ts";

export type * from "./protocol.ts";

type MaybePromise<T> = T | Promise<T>;

/// The catter whose session a handler serves, which every callback gets.
export interface Catter {
    /// What catter makes of programs the handler names, as of the commands it
    /// sends: one for each, null for a program catter knows nothing of.
    analyze(...programs: Program[]): Promise<(Analysis | null)[]>;
}

export interface Handler {
    /// Every command comes to onCommand, which returns its action, and the
    /// build waits for it. A handler that does not decide is only told.
    decides?: boolean;
    /// Commands carry their environment.
    env?: boolean;
    /// Commands carry what catter makes of them, in `command.analysis`.
    analysis?: boolean;
    /// The commands the handler is told of: those a pattern names. Every
    /// command when absent.
    filter?: Pattern[];
    /// The session starts: what it runs, and how it captures commands.
    onStart?(session: InitializeParams, catter: Catter): MaybePromise<void>;
    /// A command is about to run; returning nothing runs it as it is.
    onCommand?(command: Command, catter: Catter): MaybePromise<Action | undefined | void>;
    /// catter could not capture a command.
    onError?(error: CaptureError, catter: Catter): MaybePromise<void>;
    /// A command has run.
    onExit?(exit: Exit, catter: Catter): MaybePromise<void>;
    /// The build has ended.
    onFinish?(finish: Finish, catter: Catter): MaybePromise<void>;
}

/// A JSON-RPC error code outside the range JSON-RPC reserves: a callback threw.
const CALLBACK_FAILED = 1;

/// The protocol goes to catter over this program's stdout: from the import
/// on, what the program prints with console goes to stderr, where catter
/// shows it.
globalThis.console = new Console(process.stderr, process.stderr);

function describe(error: unknown): string {
    return error instanceof Error ? `${error.name}: ${error.message}` : String(error);
}

/// Answers catter until it ends the session. The callbacks run one after
/// another, in the order catter sent their messages: an async callback would
/// otherwise let the next one start while it waits.
function run(handler: Handler, input: Readable, output: Writable): Promise<void> {
    let queue: Promise<unknown> = Promise.resolve();
    const inTurn = <T>(callback: () => MaybePromise<T>): Promise<T> => {
        const turn = queue.then(callback);
        queue = turn.catch(() => undefined);
        return turn;
    };
    /// A request's answer, or the error it fails with when the callback throws.
    const answer = <T>(callback: () => MaybePromise<T>): Promise<T> =>
        inTurn(callback).catch((error: unknown) => {
            console.error(error);
            throw new ResponseError(CALLBACK_FAILED, describe(error));
        });
    /// Nothing answers a notification: a handler whose callback throws fails
    /// as a program does, and catter reports that it stopped.
    const told = (callback: () => MaybePromise<void>): Promise<void> =>
        inTurn(callback).catch((error: unknown) => {
            console.error(error);
            process.exit(1);
        });

    const connection = createMessageConnection(
        new StreamMessageReader(input),
        new StreamMessageWriter(output),
    );
    const catter: Catter = {
        async analyze(...programs) {
            const params: AnalyzeParams = { programs };
            const result: AnalyzeResult = await connection.sendRequest("analyze", params);
            return result.analyses;
        },
    };
    connection.onRequest("initialize", (session: InitializeParams): Promise<InitializeResult> =>
        answer(async () => {
            await handler.onStart?.(session, catter);
            return {
                decides: handler.decides ?? false,
                events: {
                    command: handler.onCommand !== undefined,
                    error: handler.onError !== undefined,
                    exit: handler.onExit !== undefined,
                },
                env: handler.env ?? false,
                analysis: handler.analysis ?? false,
                filter: handler.filter ?? null,
            };
        }),
    );
    connection.onRequest("command", (command: Command): Promise<Action> =>
        answer(async () => (await handler.onCommand?.(command, catter)) ?? { action: "skip" }),
    );
    connection.onRequest("finish", (finish: Finish): Promise<null> =>
        answer(async () => {
            await handler.onFinish?.(finish, catter);
            return null;
        }),
    );
    connection.onNotification("command", (command: Command) =>
        told(async () => {
            await handler.onCommand?.(command, catter);
        }),
    );
    connection.onNotification("error", (error: CaptureError) =>
        told(() => handler.onError?.(error, catter)),
    );
    connection.onNotification("exit", (exit: Exit) => told(() => handler.onExit?.(exit, catter)));

    const closed = new Promise<void>((resolve) => connection.onClose(() => resolve()));
    connection.listen();
    return closed;
}

/// Serves the session of the catter that started this program
/// (`--handler`), over stdin and stdout. Neither the program, with
/// process.stdout, nor a child process it starts may write to its stdout.
export function serve(handler: Handler): Promise<void> {
    return run(handler, process.stdin, process.stdout);
}

/// A TCP port catter connects to (`--connect host:port`).
export class Listener {
    /// The port listened on: the one asked for, or the one the system chose
    /// for 0.
    readonly port: number;
    private readonly server: net.Server;
    /// Connections no accept() has taken yet, and accept()s waiting for one.
    private readonly connections: net.Socket[] = [];
    private readonly waiting: ((socket: net.Socket) => void)[] = [];

    constructor(server: net.Server) {
        this.server = server;
        this.port = (server.address() as net.AddressInfo).port;
        server.on("connection", (socket) => {
            const take = this.waiting.shift();
            if (take === undefined) {
                this.connections.push(socket);
            } else {
                take(socket);
            }
        });
    }

    /// Serves the session of the next catter that connects.
    async accept(handler: Handler): Promise<void> {
        const socket =
            this.connections.shift() ??
            (await new Promise<net.Socket>((resolve) => this.waiting.push(resolve)));
        await run(handler, socket, socket);
        socket.end();
    }

    close(): void {
        this.server.close();
    }
}

/// Listens at host:port for catter to connect to.
export function listen(port = 0, host = "127.0.0.1"): Promise<Listener> {
    return new Promise((resolve, reject) => {
        const server = net.createServer();
        server.once("error", reject);
        server.listen(port, host, () => resolve(new Listener(server)));
    });
}
