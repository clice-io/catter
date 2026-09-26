"""Helpers for the end-to-end tests that run catter over real builds.

prepare <project> <dest>
    Copy a project from Inputs/ into a fresh directory, so every run builds
    from a clean tree and the sources stay untouched.

summarize <compile_commands.json> <root>
    Print the database in a platform-neutral form for FileCheck: one line per
    entry for the sources under <root>, sorted, with paths relative to <root>
    and '/' as the separator, after a count of the entries for sources
    elsewhere (such as the ones build tools compile to probe the compiler).
"""

import json
import os
import shutil
import sys


def prepare(project: str, dest: str) -> None:
    source = os.path.join(os.path.dirname(os.path.abspath(__file__)), project)
    shutil.rmtree(dest, ignore_errors=True)
    shutil.copytree(source, dest)


def relative(path: str, root: str) -> str:
    # realpath on both sides: macOS reports /tmp as /private/tmp.
    return os.path.relpath(os.path.realpath(path), os.path.realpath(root)).replace(
        os.sep, "/"
    )


def has_define(arguments: list[str]) -> bool:
    return any(arg.lstrip("-/") == "DCATTER_E2E=1" for arg in arguments)


def summarize(database: str, root: str) -> None:
    with open(database, encoding="utf-8") as f:
        entries = json.load(f)

    lines = []
    others = 0
    for entry in entries:
        directory = entry["directory"]
        file = os.path.join(directory, entry["file"])
        if relative(file, root).startswith("../"):
            others += 1
            continue
        output = entry.get("output")
        fields = [
            relative(file, root),
            "exists" if os.path.isfile(file) else "missing",
            f"dir={relative(directory, root)}",
            f"output={relative(os.path.join(directory, output), root) if output else '-'}",
            f"define={'yes' if has_define(entry['arguments']) else 'no'}",
        ]
        lines.append(" ".join(fields))

    print(f"entries: {len(lines)}")
    print(f"other entries: {others}")
    for line in sorted(lines):
        print(line)


def main() -> None:
    match sys.argv[1:]:
        case ["prepare", project, dest]:
            prepare(project, dest)
        case ["summarize", database, root]:
            summarize(database, root)
        case _:
            sys.exit(__doc__)


if __name__ == "__main__":
    main()
