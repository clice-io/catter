# Generated from sdk/protocol.schema.json by sdk/generate.py: do not edit.
"""The messages catter and a handler exchange."""

from __future__ import annotations

import base64
from dataclasses import dataclass
from typing import Any


def _os_from(value: Any) -> str:
    """Bytes from the system as Python spells them: undecodable bytes as
    lone surrogates, as os.fsdecode does."""
    if isinstance(value, str):
        return value
    raw = base64.b64decode(value["base64"], validate=True)
    return raw.decode("utf-8", "surrogateescape")


def _os_to(value: str) -> Any:
    try:
        value.encode("utf-8")
        return value
    except UnicodeEncodeError:
        raw = value.encode("utf-8", "surrogateescape")
        return {"base64": base64.b64encode(raw).decode("ascii")}


@dataclass
class Build:
    command: list[str]
    cwd: str

    @staticmethod
    def from_json(data: dict[str, Any]) -> Build:
        return Build(
            command=[_os_from(item) for item in data["command"]],
            cwd=_os_from(data["cwd"]),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["command"] = [_os_to(item) for item in self.command]
        data["cwd"] = _os_to(self.cwd)
        return data


@dataclass
class InitializeParams:
    version: int
    session: str
    mode: str
    build: Build

    @staticmethod
    def from_json(data: dict[str, Any]) -> InitializeParams:
        return InitializeParams(
            version=data["version"],
            session=data["session"],
            mode=data["mode"],
            build=Build.from_json(data["build"]),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["version"] = self.version
        data["session"] = self.session
        data["mode"] = self.mode
        data["build"] = self.build.to_json()
        return data


@dataclass
class Events:
    command: bool
    error: bool
    exit: bool

    @staticmethod
    def from_json(data: dict[str, Any]) -> Events:
        return Events(
            command=data["command"],
            error=data["error"],
            exit=data["exit"],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["command"] = self.command
        data["error"] = self.error
        data["exit"] = self.exit
        return data


@dataclass
class InitializeResult:
    decides: bool
    events: Events
    env: bool

    @staticmethod
    def from_json(data: dict[str, Any]) -> InitializeResult:
        return InitializeResult(
            decides=data["decides"],
            events=Events.from_json(data["events"]),
            env=data["env"],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["decides"] = self.decides
        data["events"] = self.events.to_json()
        data["env"] = self.env
        return data


@dataclass
class Command:
    id: int
    cwd: str
    executable: str
    args: list[str]
    parent: int | None = None
    env: list[str] | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Command:
        return Command(
            id=data["id"],
            cwd=_os_from(data["cwd"]),
            executable=_os_from(data["executable"]),
            args=[_os_from(item) for item in data["args"]],
            parent=data.get("parent"),
            env=None if data.get("env") is None else [_os_from(item) for item in data.get("env")],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["id"] = self.id
        data["cwd"] = _os_to(self.cwd)
        data["executable"] = _os_to(self.executable)
        data["args"] = [_os_to(item) for item in self.args]
        if self.parent is not None:
            data["parent"] = self.parent
        if self.env is not None:
            data["env"] = [_os_to(item) for item in self.env]
        return data


@dataclass
class CaptureError:
    id: int
    message: str
    parent: int | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> CaptureError:
        return CaptureError(
            id=data["id"],
            message=data["message"],
            parent=data.get("parent"),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["id"] = self.id
        data["message"] = self.message
        if self.parent is not None:
            data["parent"] = self.parent
        return data


@dataclass
class Exit:
    id: int
    code: int
    duration: int

    @staticmethod
    def from_json(data: dict[str, Any]) -> Exit:
        return Exit(
            id=data["id"],
            code=data["code"],
            duration=data["duration"],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["id"] = self.id
        data["code"] = self.code
        data["duration"] = self.duration
        return data


@dataclass
class Finish:
    code: int

    @staticmethod
    def from_json(data: dict[str, Any]) -> Finish:
        return Finish(
            code=data["code"],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["code"] = self.code
        return data


@dataclass
class Skip:
    pass

    @staticmethod
    def from_json(data: dict[str, Any]) -> Skip:
        return Skip()

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"action": "skip"}
        return data


@dataclass
class Drop:
    pass

    @staticmethod
    def from_json(data: dict[str, Any]) -> Drop:
        return Drop()

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"action": "drop"}
        return data


@dataclass
class Abort:
    reason: str

    @staticmethod
    def from_json(data: dict[str, Any]) -> Abort:
        return Abort(
            reason=data["reason"],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"action": "abort"}
        data["reason"] = self.reason
        return data


@dataclass
class Modify:
    args: list[str]
    executable: str | None = None
    cwd: str | None = None
    env: list[str] | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Modify:
        return Modify(
            args=[_os_from(item) for item in data["args"]],
            executable=None if data.get("executable") is None else _os_from(data.get("executable")),
            cwd=None if data.get("cwd") is None else _os_from(data.get("cwd")),
            env=None if data.get("env") is None else [_os_from(item) for item in data.get("env")],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"action": "modify"}
        data["args"] = [_os_to(item) for item in self.args]
        if self.executable is not None:
            data["executable"] = _os_to(self.executable)
        if self.cwd is not None:
            data["cwd"] = _os_to(self.cwd)
        if self.env is not None:
            data["env"] = [_os_to(item) for item in self.env]
        return data


Action = Skip | Drop | Abort | Modify


def action_from_json(data: dict[str, Any]) -> Action:
    match data["action"]:
        case "skip":
            return Skip.from_json(data)
        case "drop":
            return Drop.from_json(data)
        case "abort":
            return Abort.from_json(data)
        case "modify":
            return Modify.from_json(data)
    raise ValueError(f"unknown action: {data['action']}")
