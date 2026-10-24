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
/// and `catter --handler "node handler.ts" -- make` runs the build with it.
/// `listen()` serves catter's `--connect` instead.
///
/// Every callback is optional: a handler is told of the events it has a
/// callback for. Once the SDK is imported, what the program prints goes to
/// stderr: its stdout carries the protocol. An argument, a path or an environment entry that is not
/// UTF-8 comes as `{ base64 }`, which a command handed back keeps.

import * as fs from "node:fs";
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
    CaptureError,
    Command,
    Exit,
    Finish,
    InitializeParams,
    InitializeResult,
} from "./protocol.ts";

export type * from "./protocol.ts";

type MaybePromise<T> = T | Promise<T>;

export interface Handler {
    /// Every command comes to onCommand, which returns its action, and the
    /// build waits for it. A handler that does not decide is only told.
    decides?: boolean;
    /// Commands carry their environment.
    env?: boolean;
    /// The session starts: what it runs, and how it captures commands.
    onStart?(session: InitializeParams): MaybePromise<void>;
    /// A command is about to run; returning nothing runs it as it is.
    onCommand?(command: Command): MaybePromise<Action | undefined | void>;
    /// catter could not capture a command.
    onError?(error: CaptureError): MaybePromise<void>;
    /// A command has run.
    onExit?(exit: Exit): MaybePromise<void>;
    /// The build has ended.
    onFinish?(finish: Finish): MaybePromise<void>;
}

/// A JSON-RPC error code outside the range JSON-RPC reserves: a callback threw.
const CALLBACK_FAILED = 1;

/// The protocol goes to catter over this program's stdout: from the import
/// on, what the program prints with console.log or process.stdout goes to
/// stderr, where catter shows it.
const protocolOutput = fs.createWriteStream("", { fd: process.stdout.fd });
process.stdout.write = process.stderr.write.bind(process.stderr) as typeof process.stdout.write;

function describe(error: unknown): string {
    return error instanceof Error ? `${error.name}: ${error.message}` : String(error);
}

/// A request's answer, or the error it fails with when the callback throws.
async function answer<T>(callback: () => MaybePromise<T>): Promise<T> {
    try {
        return await callback();
    } catch (error) {
        console.error(error);
        throw new ResponseError(CALLBACK_FAILED, describe(error));
    }
}

/// Nothing answers a notification: a handler whose callback throws fails as
/// a program does, and catter reports that it stopped.
async function told(callback: () => MaybePromise<void>): Promise<void> {
    try {
        await callback();
    } catch (error) {
        console.error(error);
        process.exit(1);
    }
}

/// Answers catter until it ends the session.
function run(handler: Handler, input: Readable, output: Writable): Promise<void> {
    const connection = createMessageConnection(
        new StreamMessageReader(input),
        new StreamMessageWriter(output),
    );
    connection.onRequest("initialize", (session: InitializeParams): Promise<InitializeResult> =>
        answer(async () => {
            await handler.onStart?.(session);
            return {
                decides: handler.decides ?? false,
                events: {
                    command: handler.onCommand !== undefined,
                    error: handler.onError !== undefined,
                    exit: handler.onExit !== undefined,
                },
                env: handler.env ?? false,
            };
        }),
    );
    connection.onRequest("command", (command: Command): Promise<Action> =>
        answer(async () => (await handler.onCommand?.(command)) ?? { action: "skip" }),
    );
    connection.onRequest("finish", (finish: Finish): Promise<null> =>
        answer(async () => {
            await handler.onFinish?.(finish);
            return null;
        }),
    );
    connection.onNotification("command", (command: Command) =>
        told(async () => {
            await handler.onCommand?.(command);
        }),
    );
    connection.onNotification("error", (error: CaptureError) =>
        told(() => handler.onError?.(error)),
    );
    connection.onNotification("exit", (exit: Exit) => told(() => handler.onExit?.(exit)));

    const closed = new Promise<void>((resolve) => connection.onClose(() => resolve()));
    connection.listen();
    return closed;
}

/// Serves the session of the catter that started this program
/// (`--handler`), over stdin and stdout. A child process it starts must not
/// inherit its stdout.
export function serve(handler: Handler): Promise<void> {
    return run(handler, process.stdin, protocolOutput);
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
