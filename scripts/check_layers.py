#!/usr/bin/env python3
"""Enforce the src/ include layering: {support, command} <- session <- {capture,
protocol, feature} <- driver; command, to become a library of its own, builds on
nothing of src/, proxy on support, session and capture, and the hook payloads on
support alone.

Every include names its header by the path under src/, so the directory it starts
with is the layer it comes from; a header of src/ included by any other path is
reported too, as it would hide its layer. It needs no build.
"""

import re
import sys
from pathlib import Path

# Directory -> the directories its sources may include, besides its own.
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

INCLUDE = re.compile(r'^\s*#\s*include\s+(?:"([^"]+)"|<([^>]+)>)')


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    src = root / "src"
    violations = []
    for path in sorted(src.rglob("*")):
        if path.suffix not in (".h", ".cc"):
            continue
        layer = path.relative_to(src).parts[0]
        if layer not in ALLOWED:
            violations.append(f"{path.relative_to(root)}: src/{layer} is in no layer")
            continue
        allowed = [layer, *ALLOWED[layer]]
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            quoted, header = (
                match.group(1) is not None,
                match.group(1) or match.group(2),
            )
            where = f"{path.relative_to(root)}:{number}"
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
            "includes name their header by its path under src/."
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
