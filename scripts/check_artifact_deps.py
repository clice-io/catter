#!/usr/bin/env python3
"""Check that a release package of catter depends on the system's libraries alone.

Every program and hook payload in the package (catter.tar.gz or catter.zip) is
read for its dynamic dependencies: a C++ runtime, or any other library a
system does not have, means a user's machine cannot run it. xclang links its
libc++ statically into each of them; the hook payloads run inside every
process of a build, beside that program's own C++ runtime, so one of theirs
must not be shared either.

Each format is read with LLVM's tools (``llvm-readelf``, ``llvm-otool``,
``llvm-readobj``), which xclang's environment has, and which read every
format on every host.
"""

import argparse
import re
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

# The glibc libraries a Linux program may need. libstdc++.so.6, libc++.so and
# libgcc_s.so.1 are absent: one of them would come from the build machine.
LINUX_NEEDED_ALLOWED = {
    "libpthread.so.0",
    "librt.so.1",
    "libdl.so.2",
    "libm.so.6",
    "libc.so.6",
    "ld-linux-x86-64.so.2",
}

# MinGW's runtime DLLs are named lib* (libc++.dll, libunwind.dll,
# libwinpthread-1.dll, libgcc_s_seh-1.dll, libstdc++-6.dll), which no Windows
# system DLL is.
PE_FORBIDDEN_PREFIX = "lib"

MACOS_SYSTEM_PREFIXES = ("/usr/lib/", "/System/Library/")
# The SDK's libc++ is a system library, but a dependency on it means the link
# took it in place of xclang's.
MACOS_FORBIDDEN_DYLIBS = ("libc++.", "libc++abi.")


def run_tool(args: list[str]) -> str:
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"{args[0]} failed (exit {result.returncode}):\n{result.stderr}"
        )
    return result.stdout


def parsed_none(count: int, tool: str, what: str) -> list[str]:
    """Every program and payload depends on the C library at least: none parsed
    means the tool's output changed, not that the file is clean."""
    if count:
        return []
    return [f"parsed no {what} from `{tool}`; its output format may have changed"]


def check_elf(binary: Path) -> list[str]:
    out = run_tool(["llvm-readelf", "-d", str(binary)])
    violations = []
    parsed = 0
    for needed in re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", out):
        parsed += 1
        if needed not in LINUX_NEEDED_ALLOWED:
            violations.append(f"NEEDED {needed}")
    return violations + parsed_none(parsed, "llvm-readelf -d", "NEEDED entries")


def check_macho(binary: Path) -> list[str]:
    violations = []
    parsed = 0
    # A dylib's own install name, which -L lists among its dependencies; both
    # print the file's path first.
    own = run_tool(["llvm-otool", "-D", str(binary)]).splitlines()[1:]
    for line in run_tool(["llvm-otool", "-L", str(binary)]).splitlines()[1:]:
        line = line.strip()
        if not line.endswith(")"):
            continue
        dylib = line.split(" (", 1)[0].strip()
        if dylib in own:
            continue
        parsed += 1
        if not dylib.startswith(MACOS_SYSTEM_PREFIXES):
            violations.append(f"non-system dylib {dylib}")
        elif Path(dylib).name.startswith(MACOS_FORBIDDEN_DYLIBS):
            violations.append(f"C++ runtime dylib {dylib}")
    return violations + parsed_none(parsed, "llvm-otool -L", "dylib dependencies")


def check_pe(binary: Path) -> list[str]:
    out = run_tool(["llvm-readobj", "--coff-imports", str(binary)])
    dlls = re.findall(r"^\s*Name: (\S+\.dll)\s*$", out, re.MULTILINE | re.IGNORECASE)
    violations = [
        f"runtime DLL {dll}"
        for dll in dlls
        if dll.lower().startswith(PE_FORBIDDEN_PREFIX)
    ]
    return violations + parsed_none(
        len(dlls), "llvm-readobj --coff-imports", "imported DLLs"
    )


CHECKS = {
    b"\x7fELF": check_elf,
    b"\xcf\xfa\xed\xfe": check_macho,
    b"MZ": check_pe,
}


def check(binary: Path) -> list[str]:
    magic = binary.read_bytes()[:4]
    for prefix, check_format in CHECKS.items():
        if magic.startswith(prefix):
            return check_format(binary)
    return [f"not an ELF, Mach-O or PE file (magic {magic!r})"]


def unpack(package: Path, into: Path) -> None:
    if package.name.endswith(".zip"):
        with zipfile.ZipFile(package) as archive:
            archive.extractall(into)
    else:
        with tarfile.open(package) as archive:
            archive.extractall(into, filter="data")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "package",
        type=Path,
        help="catter.tar.gz or catter.zip, as //:package builds it",
    )
    package = parser.parse_args().package

    failed = False
    with tempfile.TemporaryDirectory() as directory:
        unpack(package, Path(directory))
        binaries = sorted(
            path for path in Path(directory).glob("catter/bin/*") if path.is_file()
        )
        if not binaries:
            print(f"FAIL: {package} has nothing in catter/bin", file=sys.stderr)
            return 1
        for binary in binaries:
            violations = check(binary)
            if violations:
                failed = True
                print(f"FAIL: {binary.name}", file=sys.stderr)
                for violation in violations:
                    print(f"  - {violation}", file=sys.stderr)
            else:
                print(f"OK: {binary.name} depends on the system's libraries alone")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
