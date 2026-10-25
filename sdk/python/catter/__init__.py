"""Handlers for catter, in Python's standard library alone.

A handler is told of the commands a build under catter starts and, if it
decides, answers each with what catter does with it::

    import catter

    class Handler(catter.Handler):
        decides = True

        def on_command(self, command):
            if "-O0" in command.args:
                return catter.Modify(args=[*command.args, "-g"])
            return None  # catter.Skip(): run it as it is

    catter.serve(Handler())

and `catter --handler "python handler.py" -- make` runs the build with it.
`listen()` serves catter's `--connect` instead.

A handler may ask for what catter makes of each command (`analysis = True`),
be told of only some commands (`filter`), edit a command's arguments
(`catter.Edit`), and ask what catter makes of commands of its own
(`self.analyze`).

Every callback is optional: a handler is told of the events whose callback
it overrides. Once catter is imported, what the program prints goes to
stderr: its stdout carries the protocol. Arguments, paths and environment entries are strings, with the
bytes that are not UTF-8 as lone surrogates, as os.fsdecode spells them.
"""

from __future__ import annotations

import json
import os
import socket
import sys
import traceback
from collections import deque
from pathlib import Path
from typing import Any, BinaryIO

from .protocol import (
    Abort,
    Action,
    Analysis,
    AnalyzeParams,
    AnalyzeResult,
    ArchiverAnalysis,
    Argument,
    Build,
    CaptureError,
    Command,
    CommandEdit,
    CompilerAnalysis,
    Diagnostic,
    Drop,
    Edit,
    Events,
    Exit,
    Finish,
    InitializeParams,
    InitializeResult,
    Modify,
    Pattern,
    Program,
    Read,
    Skip,
    Target,
    ToolAnalysis,
    UnparsedAnalysis,
    Write,
)

__all__ = [
    "Abort",
    "Action",
    "Analysis",
    "ArchiverAnalysis",
    "Argument",
    "Build",
    "CaptureError",
    "Command",
    "CommandEdit",
    "CompilerAnalysis",
    "Diagnostic",
    "Drop",
    "Edit",
    "Exit",
    "Finish",
    "Handler",
    "InitializeParams",
    "Listener",
    "Modify",
    "Pattern",
    "Program",
    "Read",
    "Skip",
    "Target",
    "ToolAnalysis",
    "UnparsedAnalysis",
    "Write",
    "listen",
    "serve",
]

# A JSON-RPC error code outside the range JSON-RPC reserves: a callback raised.
CALLBACK_FAILED = 1

_SDK = Path(__file__).resolve().parent

# The protocol goes to catter over this program's stdout: from the import on,
# what the program prints, and what the programs it starts print, goes to
# stderr, where catter shows it, and as stderr takes it: a character the
# encoding cannot spell is escaped, not an error.
sys.stdout.flush()
_protocol_output = os.fdopen(os.dup(sys.stdout.fileno()), "wb")
os.dup2(sys.stderr.fileno(), sys.stdout.fileno())
sys.stdout.reconfigure(line_buffering=True, errors="backslashreplace")


class Handler:
    """What a handler does with a session; subclass it and override the
    callbacks the handler needs."""

    #: Every command comes to on_command, which returns its action, and the
    #: build waits for it. A handler that does not decide is only told.
    decides: bool = False

    #: Commands carry their environment.
    env: bool = False

    #: Commands carry what catter makes of them, in `command.analysis`.
    analysis: bool = False

    #: The commands the handler is told of: those a pattern names. Every
    #: command when None.
    filter: list[Pattern] | None = None

    _session: _Session | None = None

    def analyze(self, *programs: Program) -> list[Analysis | None]:
        """What catter makes of programs the handler names, as of the
        commands it sends: one for each, None for a program catter knows
        nothing of."""
        if self._session is None:
            raise RuntimeError(
                "analyze() asks the catter whose session the handler serves"
            )
        params = AnalyzeParams(programs=list(programs)).to_json()
        return AnalyzeResult.from_json(
            self._session.request("analyze", params)
        ).analyses

    def on_start(self, session: InitializeParams) -> None:
        """The session starts: what it runs, and how it captures commands."""

    def on_command(self, command: Command) -> Action | None:
        """A command is about to run. Returning None runs it as it is."""

    def on_error(self, error: CaptureError) -> None:
        """catter could not capture a command."""

    def on_exit(self, exit: Exit) -> None:
        """A command has run."""

    def on_finish(self, finish: Finish) -> None:
        """The build has ended."""


def _print_failure(error: BaseException) -> None:
    """The traceback of a callback that raised, from the callback on."""
    frames = error.__traceback__
    while (
        frames is not None
        and Path(frames.tb_frame.f_code.co_filename).resolve().parent == _SDK
    ):
        frames = frames.tb_next
    traceback.print_exception(type(error), error, frames)


def _overrides(handler: Handler, name: str) -> bool:
    return getattr(type(handler), name) is not getattr(Handler, name)


def _read(reader: BinaryIO) -> Any | None:
    """The next message, or None at the end of the input."""
    length = None
    while True:
        line = reader.readline()
        if not line:
            return None
        line = line.rstrip(b"\r\n")
        if not line:
            break
        name, _, value = line.partition(b":")
        if name.strip().lower() == b"content-length":
            length = int(value.strip())
    if length is None:
        raise ValueError("a message without Content-Length")
    return json.loads(reader.read(length).decode("utf-8"))


def _text(value: Any) -> Any:
    """`value` with the bytes that are not UTF-8 of its strings, such as a
    message built from an argument, replaced: the protocol's strings are
    valid Unicode, and the arguments in it already base64."""
    if isinstance(value, str):
        try:
            data = value.encode("utf-8", "surrogateescape")
        except UnicodeEncodeError:
            # A lone surrogate that stands for no byte.
            data = value.encode("utf-8", "surrogatepass")
        return data.decode("utf-8", "replace")
    if isinstance(value, dict):
        return {key: _text(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_text(item) for item in value]
    return value


def _write(writer: BinaryIO, message: dict[str, Any]) -> None:
    text = json.dumps(_text(message), ensure_ascii=False, separators=(",", ":"))
    body = text.encode("utf-8")
    writer.write(b"Content-Length: %d\r\n\r\n" % len(body))
    writer.write(body)
    writer.flush()


class _Session:
    """A session with catter: its messages, and the handler's."""

    def __init__(self, reader: BinaryIO, writer: BinaryIO) -> None:
        self.reader = reader
        self.writer = writer
        # catter's messages read while a request of the handler's waited for
        # its answer.
        self.early: deque[Any] = deque()
        self.requests = 0

    def next(self) -> Any | None:
        """catter's next message, or None at the end of the session."""
        return self.early.popleft() if self.early else _read(self.reader)

    def request(self, method: str, params: Any) -> Any:
        """The result of the handler's request to catter."""
        self.requests += 1
        id = f"handler-{self.requests}"
        _write(
            self.writer,
            {"jsonrpc": "2.0", "id": id, "method": method, "params": params},
        )
        while (message := _read(self.reader)) is not None:
            if "method" in message or message.get("id") != id:
                self.early.append(message)
            elif "error" in message:
                raise RuntimeError(
                    f"catter failed on {method}: {message['error']['message']}"
                )
            else:
                return message.get("result")
        raise EOFError(f"catter ended the session before it answered {method}")


def _dispatch(handler: Handler, method: str, params: Any) -> Any:
    match method:
        case "initialize":
            handler.on_start(InitializeParams.from_json(params))
            return InitializeResult(
                decides=handler.decides,
                events=Events(
                    command=_overrides(handler, "on_command"),
                    error=_overrides(handler, "on_error"),
                    exit=_overrides(handler, "on_exit"),
                ),
                env=handler.env,
                analysis=handler.analysis,
                filter=handler.filter,
            ).to_json()
        case "command":
            action = handler.on_command(Command.from_json(params))
            return (action or Skip()).to_json()
        case "error":
            handler.on_error(CaptureError.from_json(params))
        case "exit":
            handler.on_exit(Exit.from_json(params))
        case "finish":
            handler.on_finish(Finish.from_json(params))
    return None


def _run(handler: Handler, reader: BinaryIO, writer: BinaryIO) -> None:
    """Answers catter until it ends the session."""
    session = _Session(reader, writer)
    handler._session = session
    while (message := session.next()) is not None:
        request = "id" in message
        try:
            result = _dispatch(handler, message["method"], message.get("params"))
        except Exception as error:
            _print_failure(error)
            if not request:
                # Nothing answers a notification: the handler fails as a
                # program does, and catter reports that it stopped.
                sys.exit(1)
            reply = {
                "code": CALLBACK_FAILED,
                "message": f"{type(error).__name__}: {error}",
            }
            _write(writer, {"jsonrpc": "2.0", "id": message["id"], "error": reply})
            continue
        if request:
            _write(writer, {"jsonrpc": "2.0", "id": message["id"], "result": result})


def serve(handler: Handler) -> None:
    """Serves the session of the catter that started this program
    (`--handler`), over stdin and stdout."""
    # The protocol comes from catter over stdin: the program, and the programs
    # it starts, read the null device there instead, lest one read catter's
    # messages.
    protocol_input = os.fdopen(os.dup(sys.stdin.fileno()), "rb")
    with open(os.devnull, "rb") as null:
        os.dup2(null.fileno(), sys.stdin.fileno())
    _run(handler, protocol_input, _protocol_output)


class Listener:
    """A TCP port catter connects to (`--connect host:port`)."""

    def __init__(self, host: str, port: int) -> None:
        family = socket.AF_INET6 if ":" in host else socket.AF_INET
        self._socket = socket.create_server((host, port), family=family)
        #: The port listened on: the one asked for, or the one the system
        #: chose for 0.
        self.port: int = self._socket.getsockname()[1]

    def accept(self, handler: Handler) -> None:
        """Serves the session of the next catter that connects."""
        connection, _ = self._socket.accept()
        with connection, connection.makefile("rb") as reader:
            with connection.makefile("wb") as writer:
                _run(handler, reader, writer)

    def close(self) -> None:
        self._socket.close()

    def __enter__(self) -> Listener:
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()


def listen(host: str = "127.0.0.1", port: int = 0) -> Listener:
    """Listens at host:port for catter to connect to."""
    return Listener(host, port)
