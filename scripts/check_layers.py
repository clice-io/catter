#!/usr/bin/env python3
"""Enforce the src/ layering: {support, command} <- session <- {capture, protocol,
feature} <- driver; command, to become a library of its own, builds on nothing of
src/, proxy on support, session and capture, and the hook payloads on support alone.

Every include names its header by the path under src/, so the directory it starts
with is the layer it comes from; a header of src/ included by any other path is
reported too, as it would hide its layer. Above support the code is C++20 modules:
command is catter.command, whose partitions are named by their path under
src/command (`import :option.clang;`), and the rest is catter, whose partitions are
named by their path under src/ (`import :session.process;`); the check verifies the
names too, so an import tells its layer as an include does. It needs no build.
"""

import re
import sys
from pathlib import Path

# Directory -> the directories its sources may include or import, besides its own.
ALLOWED = {
    "support": [],
    "command": [],
    "session": ["support", "command"],
    "capture": ["support", "command", "session"],
    "protocol": ["support", "command", "session"],
    "feature": ["support", "command", "session"],
    "driver": ["support", "command", "session", "capture", "protocol", "feature"],
    "proxy": ["support", "session", "capture"],
    "hook": ["support"],
}

# The primary interfaces, which import nothing.
PRIMARY = {"module.cppm": "catter", "command/module.cppm": "catter.command"}

INCLUDE = re.compile(r'^\s*#\s*include\s+(?:"([^"]+)"|<([^>]+)>)')
IMPORT = re.compile(r"^\s*(?:export\s+)?import\s+(catter[\w.]*)?(?::([\w.]+))?\s*;")
MODULE = re.compile(r"^\s*(?:export\s+)?module\s+(catter[\w.]*)(?::([\w.]+))?\s*;")


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    src = root / "src"
    violations = []
    for path in sorted(src.rglob("*")):
        if path.suffix not in (".h", ".cc", ".cppm"):
            continue
        relative = path.relative_to(src)
        if relative.as_posix() in PRIMARY:
            continue
        layer = relative.parts[0]
        if layer not in ALLOWED:
            violations.append(f"{path.relative_to(root)}: src/{layer} is in no layer")
            continue
        allowed = [layer, *ALLOWED[layer]]
        if path.suffix == ".cppm":
            if layer == "command":
                module = "catter.command"
                name = ".".join(relative.relative_to("command").with_suffix("").parts)
            else:
                module = "catter"
                name = ".".join(relative.with_suffix("").parts)
            expected = f"{module}:{name}"
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            where = f"{path.relative_to(root)}:{number}"
            if path.suffix == ".cppm" and (declared := MODULE.match(line)):
                if f"{declared.group(1)}:{declared.group(2)}" != expected:
                    violations.append(
                        f"{where}: the partition of this file is {expected}"
                    )
                continue
            if imported := IMPORT.match(line):
                module, partition = imported.group(1), imported.group(2)
                if module == "catter.command" or (layer == "command" and partition):
                    first = "command"
                elif partition:
                    first = partition.split(".")[0]
                else:
                    violations.append(f"{where}: {layer}/ must not import {module}")
                    continue
                if first not in allowed:
                    violations.append(
                        f"{where}: {layer}/ must not import {module or ''}:{partition or ''}"
                    )
                continue
            match = INCLUDE.match(line)
            if not match:
                continue
            quoted, header = (
                match.group(1) is not None,
                match.group(1) or match.group(2),
            )
            first = header.split("/")[0]
            if first in ALLOWED:
                if first not in allowed:
                    violations.append(f"{where}: {layer}/ must not include {header}")
            elif quoted and (path.parent / header).is_file():
                spelling = (path.parent / header).resolve().relative_to(src).as_posix()
                violations.append(f'{where}: include "{header}" as "{spelling}"')
    for violation in violations:
        print(violation)
    if violations:
        print(
            f"\n{len(violations)} layering violation(s): "
            "{support, command} <- session <- {capture, protocol, feature} <- driver; "
            "proxy <- support, session, capture; hook <- support; "
            "includes name their header by its path under src/, and partitions are "
            "named by theirs."
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
