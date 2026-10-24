"""Generates the SDKs' protocol types from sdk/protocol.schema.json.

The schema is what `catter-protocol-schema` prints of the messages that
src/catter/core/protocol.h defines; `pixi run -e dev protocol` writes it and
runs this. With --check, nothing is written: the run fails when a generated
file is not what the schema gives.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

SDK = Path(__file__).resolve().parent
SCHEMA = SDK / "protocol.schema.json"
PYTHON = SDK / "python" / "catter" / "protocol.py"
TYPESCRIPT = SDK / "typescript" / "src" / "protocol.ts"

HEADER = "Generated from sdk/protocol.schema.json by sdk/generate.py: do not edit."


@dataclass
class Type:
    """A field's type: `kind` is int, str, bool, os (bytes from the system),
    list, optional or a message's name."""

    kind: str
    item: Type | None = None


@dataclass
class Field:
    name: str
    type: Type
    required: bool


@dataclass
class Message:
    name: str
    fields: list[Field]
    # For an alternative of a tagged union: the tag's field and value.
    tag: tuple[str, str] | None = None


def is_os_string(node: dict) -> bool:
    return node.get("anyOf") == [{"type": "string"}, {"$ref": "#/$defs/Base64"}]


def parse_type(node: dict) -> Type:
    if is_os_string(node):
        return Type("os")
    if "$ref" in node:
        return Type(node["$ref"].removeprefix("#/$defs/"))
    if "anyOf" in node:
        options = [option for option in node["anyOf"] if option != {"type": "null"}]
        if len(options) != 1:
            raise SystemExit(f"unsupported anyOf: {node}")
        return Type("optional", parse_type(options[0]))
    kinds = {"integer": "int", "string": "str", "boolean": "bool"}
    if node.get("type") in kinds:
        return Type(kinds[node["type"]])
    if node.get("type") == "array":
        return Type("list", parse_type(node["items"]))
    raise SystemExit(f"unsupported schema node: {node}")


def parse_message(name: str, node: dict, tag: str | None = None) -> Message:
    required = set(node.get("required", []))
    fields = [
        Field(field, parse_type(value), field in required)
        for field, value in node["properties"].items()
        if field != tag
    ]
    return Message(
        name, fields, (tag, node["properties"][tag]["const"]) if tag else None
    )


def parse_union(node: dict) -> tuple[str, list[Message]]:
    """A tagged union: every alternative is an object whose tag field is a
    constant, which names it."""
    alternatives = node["oneOf"]
    tag = next(
        field
        for field, value in alternatives[0]["properties"].items()
        if "const" in value
    )
    messages = [
        parse_message(alternative["properties"][tag]["const"].title(), alternative, tag)
        for alternative in alternatives
    ]
    return tag, messages


def load(schema: dict) -> tuple[list[Message], dict[str, tuple[str, list[Message]]]]:
    messages = [
        parse_message(name, node)
        for name, node in schema["$defs"].items()
        if name != "Base64"
    ]
    unions = {
        name[0].upper() + name[1:]: parse_union(node)
        for name, node in schema["properties"].items()
        if "oneOf" in node
    }
    return messages, unions


# Python


def python_type(type: Type) -> str:
    match type.kind:
        case "int" | "str" | "bool":
            return type.kind
        case "os":
            return "str"
        case "list":
            return f"list[{python_type(type.item)}]"
        case "optional":
            return f"{python_type(type.item)} | None"
        case _:
            return type.kind


def python_from(type: Type, value: str) -> str:
    match type.kind:
        case "int" | "str" | "bool":
            return value
        case "os":
            return f"_os_from({value})"
        case "list":
            return f"[{python_from(type.item, 'item')} for item in {value}]"
        case "optional":
            inner = python_from(type.item, value)
            return inner if inner == value else f"None if {value} is None else {inner}"
        case _:
            return f"{type.kind}.from_json({value})"


def python_to(type: Type, value: str) -> str:
    match type.kind:
        case "int" | "str" | "bool":
            return value
        case "os":
            return f"_os_to({value})"
        case "list":
            return f"[{python_to(type.item, 'item')} for item in {value}]"
        case "optional":
            return python_to(type.item, value)
        case _:
            return f"{value}.to_json()"


def python_class(message: Message) -> list[str]:
    fields = sorted(message.fields, key=lambda field: not field.required)
    lines = ["", "", "@dataclass", f"class {message.name}:"]
    for field in fields:
        default = "" if field.required else " = None"
        lines.append(f"    {field.name}: {python_type(field.type)}{default}")
    if not fields:
        lines.append("    pass")

    lines += [
        "",
        "    @staticmethod",
        f"    def from_json(data: dict[str, Any]) -> {message.name}:",
    ]
    if fields:
        lines.append(f"        return {message.name}(")
        for field in fields:
            value = (
                f'data["{field.name}"]'
                if field.required
                else f'data.get("{field.name}")'
            )
            lines.append(f"            {field.name}={python_from(field.type, value)},")
        lines.append("        )")
    else:
        lines.append(f"        return {message.name}()")

    lines += ["", "    def to_json(self) -> dict[str, Any]:"]
    if message.tag:
        lines.append(
            f'        data: dict[str, Any] = {{"{message.tag[0]}": "{message.tag[1]}"}}'
        )
    else:
        lines.append("        data: dict[str, Any] = {}")
    for field in fields:
        value = f"self.{field.name}"
        if field.required:
            lines.append(
                f'        data["{field.name}"] = {python_to(field.type, value)}'
            )
        else:
            lines.append(f"        if {value} is not None:")
            lines.append(
                f'            data["{field.name}"] = {python_to(field.type, value)}'
            )
    lines.append("        return data")
    return lines


def python_union(name: str, tag: str, messages: list[Message]) -> list[str]:
    lines = []
    for message in messages:
        lines += python_class(message)
    lines += [
        "",
        "",
        f"{name} = {' | '.join(message.name for message in messages)}",
        "",
        "",
        f"def {name.lower()}_from_json(data: dict[str, Any]) -> {name}:",
        f'    match data["{tag}"]:',
    ]
    for message in messages:
        lines += [
            f'        case "{message.tag[1]}":',
            f"            return {message.name}.from_json(data)",
        ]
    lines += [
        f"    raise ValueError(f\"unknown {tag}: {{data['{tag}']}}\")",
    ]
    return lines


def python_module(messages: list[Message], unions: dict) -> str:
    lines = [
        f"# {HEADER}",
        '"""The messages catter and a handler exchange."""',
        "",
        "from __future__ import annotations",
        "",
        "import base64",
        "from dataclasses import dataclass",
        "from typing import Any",
        "",
        "",
        "def _os_from(value: Any) -> str:",
        '    """Bytes from the system as Python spells them: undecodable bytes as',
        '    lone surrogates, as os.fsdecode does."""',
        "    if isinstance(value, str):",
        "        return value",
        '    raw = base64.b64decode(value["base64"], validate=True)',
        '    return raw.decode("utf-8", "surrogateescape")',
        "",
        "",
        "def _os_to(value: str) -> Any:",
        "    try:",
        '        value.encode("utf-8")',
        "        return value",
        "    except UnicodeEncodeError:",
        '        raw = value.encode("utf-8", "surrogateescape")',
        '        return {"base64": base64.b64encode(raw).decode("ascii")}',
    ]
    for message in messages:
        lines += python_class(message)
    for name, (tag, alternatives) in unions.items():
        lines += python_union(name, tag, alternatives)
    return "\n".join(lines) + "\n"


# TypeScript


def typescript_type(type: Type) -> str:
    match type.kind:
        case "int":
            return "number"
        case "str":
            return "string"
        case "bool":
            return "boolean"
        case "os":
            return "OsString"
        case "list":
            return f"{typescript_type(type.item)}[]"
        case "optional":
            return f"{typescript_type(type.item)} | null"
        case _:
            return type.kind


def typescript_interface(message: Message) -> list[str]:
    lines = ["", f"export interface {message.name} {{"]
    if message.tag:
        lines.append(f'    {message.tag[0]}: "{message.tag[1]}";')
    for field in message.fields:
        optional = "" if field.required else "?"
        lines.append(f"    {field.name}{optional}: {typescript_type(field.type)};")
    lines.append("}")
    return lines


def typescript_module(messages: list[Message], unions: dict) -> str:
    lines = [
        f"// {HEADER}",
        "// The messages catter and a handler exchange.",
        "",
        "/// Bytes that are not valid UTF-8, in base64.",
        "export interface Base64 {",
        "    base64: string;",
        "}",
        "",
        "/// Bytes from the system: a string when they are valid UTF-8.",
        "export type OsString = string | Base64;",
    ]
    for message in messages:
        lines += typescript_interface(message)
    for name, (_, alternatives) in unions.items():
        for message in alternatives:
            lines += typescript_interface(message)
        lines += [
            "",
            f"export type {name} = {' | '.join(message.name for message in alternatives)};",
        ]
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail when a generated file is out of date",
    )
    args = parser.parse_args()

    messages, unions = load(json.loads(SCHEMA.read_text(encoding="utf-8")))
    outputs = {
        PYTHON: python_module(messages, unions),
        TYPESCRIPT: typescript_module(messages, unions),
    }
    stale = [
        path
        for path, text in outputs.items()
        if not path.exists() or path.read_text(encoding="utf-8") != text
    ]
    if args.check:
        for path in stale:
            print(
                f"{path.relative_to(SDK.parent)} is out of date: run `pixi run -e dev protocol`"
            )
        return 1 if stale else 0
    for path in stale:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(outputs[path], encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
