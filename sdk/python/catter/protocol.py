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
    decisions: bool
    build: Build

    @staticmethod
    def from_json(data: dict[str, Any]) -> InitializeParams:
        return InitializeParams(
            version=data["version"],
            session=data["session"],
            mode=data["mode"],
            decisions=data["decisions"],
            build=Build.from_json(data["build"]),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["version"] = self.version
        data["session"] = self.session
        data["mode"] = self.mode
        data["decisions"] = self.decisions
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
class Pattern:
    kind: str
    names: list[str] | None = None
    phases: list[str] | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Pattern:
        return Pattern(
            kind=data["kind"],
            names=None if data.get("names") is None else [item for item in data.get("names")],
            phases=None if data.get("phases") is None else [item for item in data.get("phases")],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["kind"] = self.kind
        if self.names is not None:
            data["names"] = [item for item in self.names]
        if self.phases is not None:
            data["phases"] = [item for item in self.phases]
        return data


@dataclass
class InitializeResult:
    decides: bool
    events: Events
    env: bool
    analysis: bool
    filter: list[Pattern] | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> InitializeResult:
        return InitializeResult(
            decides=data["decides"],
            events=Events.from_json(data["events"]),
            env=data["env"],
            analysis=data["analysis"],
            filter=None if data.get("filter") is None else [Pattern.from_json(item) for item in data.get("filter")],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["decides"] = self.decides
        data["events"] = self.events.to_json()
        data["env"] = self.env
        data["analysis"] = self.analysis
        if self.filter is not None:
            data["filter"] = [item.to_json() for item in self.filter]
        return data


@dataclass
class Target:
    artifactModel: str
    source: str
    triple: str | None = None
    os: str | None = None
    objectFormat: str | None = None
    env: str | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Target:
        return Target(
            artifactModel=data["artifactModel"],
            source=data["source"],
            triple=data.get("triple"),
            os=data.get("os"),
            objectFormat=data.get("objectFormat"),
            env=data.get("env"),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["artifactModel"] = self.artifactModel
        data["source"] = self.source
        if self.triple is not None:
            data["triple"] = self.triple
        if self.os is not None:
            data["os"] = self.os
        if self.objectFormat is not None:
            data["objectFormat"] = self.objectFormat
        if self.env is not None:
            data["env"] = self.env
        return data


@dataclass
class Expansion:
    files: list[str]
    args: list[str]

    @staticmethod
    def from_json(data: dict[str, Any]) -> Expansion:
        return Expansion(
            files=[_os_from(item) for item in data["files"]],
            args=[_os_from(item) for item in data["args"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["files"] = [_os_to(item) for item in self.files]
        data["args"] = [_os_to(item) for item in self.args]
        return data


@dataclass
class Argument:
    option: str
    spelling: str
    index: int
    count: int
    values: list[str]

    @staticmethod
    def from_json(data: dict[str, Any]) -> Argument:
        return Argument(
            option=data["option"],
            spelling=_os_from(data["spelling"]),
            index=data["index"],
            count=data["count"],
            values=[_os_from(item) for item in data["values"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["option"] = self.option
        data["spelling"] = _os_to(self.spelling)
        data["index"] = self.index
        data["count"] = self.count
        data["values"] = [_os_to(item) for item in self.values]
        return data


@dataclass
class Read:
    path: str
    argument: int
    index: int
    role: str
    language: str | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Read:
        return Read(
            path=_os_from(data["path"]),
            argument=data["argument"],
            index=data["index"],
            role=data["role"],
            language=data.get("language"),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["path"] = _os_to(self.path)
        data["argument"] = self.argument
        data["index"] = self.index
        data["role"] = self.role
        if self.language is not None:
            data["language"] = self.language
        return data


@dataclass
class Write:
    path: str
    reads: list[int]
    origin: str

    @staticmethod
    def from_json(data: dict[str, Any]) -> Write:
        return Write(
            path=_os_from(data["path"]),
            reads=[item for item in data["reads"]],
            origin=data["origin"],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["path"] = _os_to(self.path)
        data["reads"] = [item for item in self.reads]
        data["origin"] = self.origin
        return data


@dataclass
class Diagnostic:
    code: str
    message: str
    path: str | None = None
    argument: int | None = None
    index: int | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Diagnostic:
        return Diagnostic(
            code=data["code"],
            message=data["message"],
            path=None if data.get("path") is None else _os_from(data.get("path")),
            argument=data.get("argument"),
            index=data.get("index"),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["code"] = self.code
        data["message"] = self.message
        if self.path is not None:
            data["path"] = _os_to(self.path)
        if self.argument is not None:
            data["argument"] = self.argument
        if self.index is not None:
            data["index"] = self.index
        return data


@dataclass
class Command:
    id: int
    cwd: str
    executable: str
    args: list[str]
    parent: int | None = None
    env: list[str] | None = None
    analysis: Analysis | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Command:
        return Command(
            id=data["id"],
            cwd=_os_from(data["cwd"]),
            executable=_os_from(data["executable"]),
            args=[_os_from(item) for item in data["args"]],
            parent=data.get("parent"),
            env=None if data.get("env") is None else [_os_from(item) for item in data.get("env")],
            analysis=None if data.get("analysis") is None else analysis_from_json(data.get("analysis")),
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
        if self.analysis is not None:
            data["analysis"] = self.analysis.to_json()
        return data


@dataclass
class CommandEdit:
    kind: str
    flags: list[str]

    @staticmethod
    def from_json(data: dict[str, Any]) -> CommandEdit:
        return CommandEdit(
            kind=data["kind"],
            flags=[_os_from(item) for item in data["flags"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["kind"] = self.kind
        data["flags"] = [_os_to(item) for item in self.flags]
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
class Program:
    executable: str
    args: list[str]
    cwd: str | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> Program:
        return Program(
            executable=_os_from(data["executable"]),
            args=[_os_from(item) for item in data["args"]],
            cwd=None if data.get("cwd") is None else _os_from(data.get("cwd")),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["executable"] = _os_to(self.executable)
        data["args"] = [_os_to(item) for item in self.args]
        if self.cwd is not None:
            data["cwd"] = _os_to(self.cwd)
        return data


@dataclass
class AnalyzeParams:
    programs: list[Program]

    @staticmethod
    def from_json(data: dict[str, Any]) -> AnalyzeParams:
        return AnalyzeParams(
            programs=[Program.from_json(item) for item in data["programs"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["programs"] = [item.to_json() for item in self.programs]
        return data


@dataclass
class AnalyzeResult:
    analyses: list[Analysis | None]

    @staticmethod
    def from_json(data: dict[str, Any]) -> AnalyzeResult:
        return AnalyzeResult(
            analyses=[None if item is None else analysis_from_json(item) for item in data["analyses"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {}
        data["analyses"] = [None if item is None else item.to_json() for item in self.analyses]
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


@dataclass
class Edit:
    edits: list[CommandEdit]

    @staticmethod
    def from_json(data: dict[str, Any]) -> Edit:
        return Edit(
            edits=[CommandEdit.from_json(item) for item in data["edits"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"action": "edit"}
        data["edits"] = [item.to_json() for item in self.edits]
        return data


Action = Skip | Drop | Abort | Modify | Edit


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
        case "edit":
            return Edit.from_json(data)
    raise ValueError(f"unknown action: {data['action']}")


@dataclass
class CompilerAnalysis:
    compiler: str
    dialect: str
    phase: str
    artifact: str
    target: Target
    arguments: list[Argument]
    reads: list[Read]
    writes: list[Write]
    diagnostics: list[Diagnostic]
    expansion: Expansion | None = None

    @staticmethod
    def from_json(data: dict[str, Any]) -> CompilerAnalysis:
        return CompilerAnalysis(
            compiler=data["compiler"],
            dialect=data["dialect"],
            phase=data["phase"],
            artifact=data["artifact"],
            target=Target.from_json(data["target"]),
            arguments=[Argument.from_json(item) for item in data["arguments"]],
            reads=[Read.from_json(item) for item in data["reads"]],
            writes=[Write.from_json(item) for item in data["writes"]],
            diagnostics=[Diagnostic.from_json(item) for item in data["diagnostics"]],
            expansion=None if data.get("expansion") is None else Expansion.from_json(data.get("expansion")),
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"kind": "compiler"}
        data["compiler"] = self.compiler
        data["dialect"] = self.dialect
        data["phase"] = self.phase
        data["artifact"] = self.artifact
        data["target"] = self.target.to_json()
        data["arguments"] = [item.to_json() for item in self.arguments]
        data["reads"] = [item.to_json() for item in self.reads]
        data["writes"] = [item.to_json() for item in self.writes]
        data["diagnostics"] = [item.to_json() for item in self.diagnostics]
        if self.expansion is not None:
            data["expansion"] = self.expansion.to_json()
        return data


@dataclass
class ArchiverAnalysis:
    operation: str
    modifiers: str
    thin: bool
    archive: str
    members: list[str]
    reads: list[str]
    writes: list[str]

    @staticmethod
    def from_json(data: dict[str, Any]) -> ArchiverAnalysis:
        return ArchiverAnalysis(
            operation=data["operation"],
            modifiers=data["modifiers"],
            thin=data["thin"],
            archive=_os_from(data["archive"]),
            members=[_os_from(item) for item in data["members"]],
            reads=[_os_from(item) for item in data["reads"]],
            writes=[_os_from(item) for item in data["writes"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"kind": "archiver"}
        data["operation"] = self.operation
        data["modifiers"] = self.modifiers
        data["thin"] = self.thin
        data["archive"] = _os_to(self.archive)
        data["members"] = [_os_to(item) for item in self.members]
        data["reads"] = [_os_to(item) for item in self.reads]
        data["writes"] = [_os_to(item) for item in self.writes]
        return data


@dataclass
class ToolAnalysis:
    tool: str
    arguments: list[Argument]

    @staticmethod
    def from_json(data: dict[str, Any]) -> ToolAnalysis:
        return ToolAnalysis(
            tool=data["tool"],
            arguments=[Argument.from_json(item) for item in data["arguments"]],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"kind": "tool"}
        data["tool"] = self.tool
        data["arguments"] = [item.to_json() for item in self.arguments]
        return data


@dataclass
class UnparsedAnalysis:
    of: str
    name: str
    error: str
    message: str

    @staticmethod
    def from_json(data: dict[str, Any]) -> UnparsedAnalysis:
        return UnparsedAnalysis(
            of=data["of"],
            name=data["name"],
            error=data["error"],
            message=data["message"],
        )

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {"kind": "unparsed"}
        data["of"] = self.of
        data["name"] = self.name
        data["error"] = self.error
        data["message"] = self.message
        return data


Analysis = CompilerAnalysis | ArchiverAnalysis | ToolAnalysis | UnparsedAnalysis


def analysis_from_json(data: dict[str, Any]) -> Analysis:
    match data["kind"]:
        case "compiler":
            return CompilerAnalysis.from_json(data)
        case "archiver":
            return ArchiverAnalysis.from_json(data)
        case "tool":
            return ToolAnalysis.from_json(data)
        case "unparsed":
            return UnparsedAnalysis.from_json(data)
    raise ValueError(f"unknown kind: {data['kind']}")
