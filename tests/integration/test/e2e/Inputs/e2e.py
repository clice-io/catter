"""Helpers for the end-to-end tests that run catter over real builds.

prepare <project> <dest>
    Copy a project from Inputs/ into a fresh directory, so every run builds
    from a clean tree and the sources stay untouched.

generate <count> <dest>
    Write a project of <count> sources, half C and half C++, with a
    CMakeLists.txt and a Makefile that build them into a static library.

summarize [--brief] <compile_commands.json> <root>
    Print the database in a platform-neutral form for FileCheck: one line per
    entry for the sources under <root>, sorted, with paths relative to <root>
    and '/' as the separator, after a count of the entries for sources
    elsewhere (such as the ones build tools compile to probe the compiler).
    --brief prints counts only.

probe <scenario> [args...]
    Run as the build command under catter: start child processes the way a
    build tool does, and check that what reaches them (arguments,
    environment, stdin, working directory) and what comes back (output, exit
    status) is what a direct run gives. Prints "PASS <scenario>" or
    "FAIL <scenario>: <details>".

concurrent -- <catter command...>
    Run two catter sessions at once, each over its own tagged children, and
    check that each session sees exactly its own commands.

absent <path>
    Print "PASS absent" if <path> does not exist.

time <label> -- <command...>
    Run a command and print how long it took; its exit code is kept.

timeout <seconds> -- <command...>
    Run a command, killing it after <seconds>; its exit code is kept.
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.abspath(__file__)
WINDOWS = sys.platform == "win32"


def prepare(project: str, dest: str) -> None:
    source = os.path.join(os.path.dirname(HERE), project)
    shutil.rmtree(dest, ignore_errors=True)
    shutil.copytree(source, dest)


def generate(count: int, dest: str) -> None:
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(os.path.join(dest, "src"))
    guard = '#ifndef CATTER_E2E\n#error "CATTER_E2E must be defined"\n#endif\n'
    for i in range(count):
        suffix = "c" if i % 2 == 0 else "cc"
        with open(os.path.join(dest, "src", f"f{i}.{suffix}"), "w") as f:
            f.write(f"{guard}int f{i}(void) {{ return {i}; }}\n")
    with open(os.path.join(dest, "CMakeLists.txt"), "w") as f:
        f.write(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(many C CXX)\n"
            'file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*")\n'
            "add_library(many STATIC ${SOURCES})\n"
            "target_compile_definitions(many PRIVATE CATTER_E2E=1)\n"
        )
    with open(os.path.join(dest, "Makefile"), "w") as f:
        f.write(
            "OBJS := $(patsubst %.c,%.o,$(wildcard src/*.c)) "
            "$(patsubst %.cc,%.o,$(wildcard src/*.cc))\n"
            "libmany.a: $(OBJS)\n"
            "\t$(AR) rcs $@ $^\n"
            "%.o: %.c\n"
            "\t$(CC) -DCATTER_E2E=1 -c $< -o $@\n"
            "%.o: %.cc\n"
            "\t$(CXX) -DCATTER_E2E=1 -c $< -o $@\n"
        )


def relative(path: str, root: str) -> str | None:
    """The path relative to root with '/' separators, or None outside root."""
    # realpath on both sides: macOS reports /tmp as /private/tmp.
    path, root = os.path.realpath(path), os.path.realpath(root)
    try:
        result = os.path.relpath(path, root)
    except ValueError:
        # On another drive, e.g. a build tool's temporary files on Windows.
        return None
    if result == os.pardir or result.startswith(os.pardir + os.sep):
        return None
    return result.replace(os.sep, "/")


def has_define(arguments: list[str]) -> bool:
    return any(arg.lstrip("-/") == "DCATTER_E2E=1" for arg in arguments)


def summarize(database: str, root: str, brief: bool) -> None:
    with open(database, encoding="utf-8") as f:
        entries = json.load(f)

    lines = []
    others = missing = undefined = unexpanded = 0
    for entry in entries:
        directory = entry["directory"]
        file = os.path.join(directory, entry["file"])
        file_relative = relative(file, root)
        if file_relative is None:
            others += 1
            continue
        exists = os.path.isfile(file)
        defined = has_define(entry["arguments"])
        missing += not exists
        undefined += not defined
        unexpanded += any(arg.startswith("@") for arg in entry["arguments"])
        output = entry.get("output")
        if output:
            output = os.path.join(directory, output)
            output = relative(output, root) or output
        fields = [
            file_relative,
            "exists" if exists else "missing",
            f"dir={relative(directory, root) or directory}",
            f"output={output or '-'}",
            f"define={'yes' if defined else 'no'}",
        ]
        lines.append(" ".join(fields))

    print(f"entries: {len(lines)}")
    print(f"other entries: {others}")
    if brief:
        print(f"missing sources: {missing}")
        print(f"without define: {undefined}")
        print(f"unexpanded response files: {unexpanded}")
        return
    for line in sorted(lines):
        print(line)


# --- probes -----------------------------------------------------------------

ARGUMENTS = [
    "plain",
    "with space",
    'quote"inside',
    "back\\slash",
    "trailing\\",
    "two\\\\",
    "",
    "unicode-中文-é",
    "tab\there",
    "a&b|c;d<e>f",
    "%PATH%",
    "$HOME",
    "*",
    "--flag=value with space",
    "'single'",
]


def child(*args: str, **kwargs) -> subprocess.CompletedProcess:
    kwargs.setdefault("capture_output", True)
    kwargs.setdefault("timeout", 300)
    return subprocess.run([sys.executable, HERE, "child", *args], **kwargs)


def run_child(mode: str, args: list[str]) -> None:
    out = sys.stdout.buffer
    match mode:
        case "argv":
            out.write(json.dumps(args).encode())
        case "env":
            out.write(
                json.dumps({name: os.environ.get(name) for name in args}).encode()
            )
        case "stdin":
            data = sys.stdin.buffer.read()
            out.write(f"{len(data)} {hashlib.sha256(data).hexdigest()}".encode())
        case "output":
            for _ in range(int(args[0])):
                sys.stdout.buffer.write(b"o" * 65536)
                sys.stdout.buffer.flush()
                sys.stderr.buffer.write(b"e" * 65536)
                sys.stderr.buffer.flush()
        case "exit":
            sys.exit(int(args[0]))
        case "abort":
            os.abort()
        case "cwd":
            out.write(json.dumps(os.getcwd()).encode())
        case "nest":
            depth = int(args[0])
            if depth == 0:
                out.write(b"leaf")
            else:
                result = child("nest", str(depth - 1))
                out.write(result.stdout)
                sys.exit(result.returncode)
        case "touch":
            open(args[0], "w").close()
        case "tag":
            pass
        case "argv0":
            out.write(sys.orig_argv[0].encode())
        case "echo":
            out.write(args[0].encode())
        case "binary":
            out.write(bytes(range(256)) * 4096)
            sys.stderr.buffer.write(bytes(range(255, -1, -1)) * 1024)
        case "write-fd":
            os.write(int(args[0]), b"through fd")
        case "sleep-pid":
            path, seconds = args
            with open(path + ".tmp", "w") as f:
                f.write(str(os.getpid()))
            os.replace(path + ".tmp", path)
            time.sleep(float(seconds))
        case _:
            sys.exit(f"unknown child mode {mode}")


def pid_alive(pid: int) -> bool:
    if WINDOWS:
        import ctypes

        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(0x1000, False, pid)  # QUERY_LIMITED_INFORMATION
        if not handle:
            return False
        code = ctypes.c_ulong()
        kernel32.GetExitCodeProcess(handle, ctypes.byref(code))
        kernel32.CloseHandle(handle)
        return code.value == 259  # STILL_ACTIVE
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def kill_pid(pid: int) -> None:
    if WINDOWS:
        subprocess.run(["taskkill", "/F", "/PID", str(pid)], capture_output=True)
    else:
        os.kill(pid, 9)


def probe(scenario: str, args: list[str]) -> str | None:
    """Run one scenario; return None when it passes, otherwise the details."""
    match scenario:
        case "argv":
            result = child("argv", *ARGUMENTS)
            got = json.loads(result.stdout or b"null")
            if got != ARGUMENTS:
                return f"sent {ARGUMENTS!r}, received {got!r}"

        case "env":
            os.environ["CATTER_PROBE_REMOVED"] = "parent only"
            env = dict(os.environ)
            del env["CATTER_PROBE_REMOVED"]
            expected = {
                "CATTER_PROBE_SPACES": "a b  c",
                "CATTER_PROBE_UNICODE": "中文-é",
                "CATTER_PROBE_EMPTY": "",
                "CATTER_PROBE_REMOVED": None,
            }
            env.update({k: v for k, v in expected.items() if v is not None})
            result = child("env", *expected, env=env)
            got = json.loads(result.stdout or b"null")
            if got != expected:
                return f"sent {expected!r}, received {got!r}"

        case "stdin":
            data = b"".join(b"line %d\n" % i for i in range(20000))
            result = child("stdin", input=data)
            expected = f"{len(data)} {hashlib.sha256(data).hexdigest()}"
            if result.stdout.decode() != expected:
                return f"expected {expected!r}, received {result.stdout!r}"

        case "output":
            chunks = 64
            result = child("output", str(chunks))
            size = chunks * 65536
            if result.stdout != b"o" * size or result.stderr != b"e" * size:
                return (
                    f"expected {size} bytes on each stream, received "
                    f"{len(result.stdout)} on stdout and {len(result.stderr)} on stderr"
                )

        case "exit-code":
            codes = [0, 1, 3, 255] + ([70000] if WINDOWS else [])
            got = [child("exit", str(code)).returncode for code in codes]
            if got != codes:
                return f"exited with {codes}, observed {got}"

        case "abort":
            code = child("abort").returncode
            if code == 0:
                return "a child that aborted was reported as exiting with 0"

        case "cwd":
            base = tempfile.mkdtemp()
            directory = os.path.join(base, "with space 中文")
            os.mkdir(directory)
            result = child("cwd", cwd=directory)
            got = json.loads(result.stdout or b"null")
            same = got is not None and os.path.samefile(got, directory)
            shutil.rmtree(base)
            if not same:
                return (
                    f"started in {directory!r}, child saw {got!r} ({result.stderr!r})"
                )

        case "long-argument":
            # Close to the 32767-character command-line limit on Windows.
            length = 32000 - len(sys.executable) - len(HERE) if WINDOWS else 100_000
            result = child("argv", "x" * length)
            got = json.loads(result.stdout or b"null")
            if got != ["x" * length]:
                return (
                    f"sent one argument of {length} characters, child exited "
                    f"with {result.returncode}: {result.stderr[-500:]!r}"
                )

        case "scrubbed-env":
            # Like `env -i`: the child gets only what it needs to start.
            names = ["PATH", "SYSTEMROOT"] if WINDOWS else ["PATH"]
            env = {name: os.environ[name] for name in names if name in os.environ}
            result = child("exit", "0", env=env)
            if result.returncode != 0:
                return (
                    f"child exited with {result.returncode}: {result.stderr[-500:]!r}"
                )

        case "nested":
            result = child("nest", "3")
            if result.returncode != 0 or result.stdout != b"leaf":
                return f"exit {result.returncode}, output {result.stdout!r}"

        case "wow64":
            cmd = os.path.join(os.environ["SYSTEMROOT"], "SysWOW64", "cmd.exe")
            result = subprocess.run(
                [cmd, "/c", "exit 7"], capture_output=True, timeout=300
            )
            if result.returncode != 7:
                return (
                    f"32-bit cmd.exe exited with {result.returncode}: {result.stderr!r}"
                )

        case "fail-then-touch":
            child("exit", "1")
            time.sleep(2)
            child("touch", args[0])

        case "missing-program":
            try:
                result = subprocess.run(
                    ["catter-e2e-no-such-program"], capture_output=True, timeout=300
                )
            except FileNotFoundError:
                pass
            else:
                return (
                    f"starting a missing program did not fail; it ran and exited with "
                    f"{result.returncode}: {result.stderr[-300:]!r}"
                )

        case "not-executable":
            directory = tempfile.mkdtemp()
            path = os.path.join(directory, "not-executable")
            with open(path, "w") as f:
                f.write("#!/bin/sh\necho ran\n")
            os.chmod(path, 0o644)
            try:
                result = subprocess.run([path], capture_output=True, timeout=300)
            except PermissionError:
                result = None
            shutil.rmtree(directory)
            if result is not None:
                return (
                    f"starting a file without execute permission did not fail; it exited "
                    f"with {result.returncode}: {result.stdout!r} {result.stderr[-300:]!r}"
                )

        case "argv0":
            result = subprocess.run(
                ["custom-argv0", HERE, "child", "argv0"],
                executable=sys.executable,
                capture_output=True,
                timeout=300,
            )
            if result.stdout != b"custom-argv0":
                return (
                    f"started with argv[0] 'custom-argv0', child saw {result.stdout!r}"
                )

        case "kill-child":
            # A build tool killing a command it started (a timeout, a cancelled
            # job) must stop the command itself.
            pidfile = os.path.join(tempfile.mkdtemp(), "pid")
            process = subprocess.Popen(
                [sys.executable, HERE, "child", "sleep-pid", pidfile, "120"]
            )
            deadline = time.time() + 120
            while not os.path.exists(pidfile) and time.time() < deadline:
                time.sleep(0.1)
            if not os.path.exists(pidfile):
                process.kill()
                return "the child never started"
            pid = int(open(pidfile).read())
            process.kill()
            process.wait()
            time.sleep(2)
            if pid_alive(pid):
                kill_pid(pid)
                return f"killed the command started as pid {process.pid}, but the program (pid {pid}) kept running"

        case "fan-out":
            count = int(args[0])
            if WINDOWS:
                command = [
                    os.path.join(os.environ["SYSTEMROOT"], "System32", "cmd.exe"),
                    "/d",
                    "/c",
                    "echo",
                ]
            else:
                command = ["/bin/echo"] if os.path.exists("/bin/echo") else ["echo"]
            processes = [
                subprocess.Popen(
                    [*command, f"n{i}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE
                )
                for i in range(count)
            ]
            failures = []
            for i, process in enumerate(processes):
                out, err = process.communicate(timeout=300)
                if process.returncode != 0 or out.strip() != f"n{i}".encode():
                    failures.append((i, process.returncode, out[-80:], err[-200:]))
            if failures:
                return f"{len(failures)} of {count} simultaneous children failed, e.g. {failures[:3]!r}"

        case "binary-output":
            result = child("binary")
            if result.stdout != bytes(range(256)) * 4096:
                return f"stdout: expected 1 MiB of all byte values, received {len(result.stdout)} bytes"
            if result.stderr != bytes(range(255, -1, -1)) * 1024:
                return f"stderr: expected 256 KiB of all byte values, received {len(result.stderr)} bytes"

        case "big-env":
            value = "v" * (30000 if WINDOWS else 100_000)
            env = dict(os.environ, CATTER_PROBE_BIG=value)
            result = child("env", "CATTER_PROBE_BIG", env=env)
            got = json.loads(result.stdout or b"null")
            if got != {"CATTER_PROBE_BIG": value}:
                size = len((got or {}).get("CATTER_PROBE_BIG") or "")
                return f"sent a {len(value)}-character variable, child saw {size} characters (exit {result.returncode})"

        case "pass-fds":
            read, write = os.pipe()
            result = child("write-fd", str(write), pass_fds=(write,))
            os.close(write)
            data = os.read(read, 100)
            os.close(read)
            if data != b"through fd":
                return f"child could not write to an inherited fd: exit {result.returncode}, {result.stderr[-300:]!r}"

        case "batch":
            directory = tempfile.mkdtemp()
            script = os.path.join(directory, "probe.bat")
            with open(script, "w") as f:
                f.write("@echo %*\n")
            result = subprocess.run(
                [script, "a b", "c"], capture_output=True, timeout=300
            )
            shutil.rmtree(directory)
            if result.stdout.strip() != b'"a b" c':
                return f"batch file printed {result.stdout!r}, exit {result.returncode}, {result.stderr[-300:]!r}"

        case "cmd-quoting":
            # cmd.exe does not split its command line like other programs, so
            # it must receive the caller's command line as written.
            line = 'cmd.exe /d /c "echo first&& echo "quoted arg"&& echo third"'
            result = subprocess.run(line, capture_output=True, timeout=300)
            got = result.stdout.decode(errors="replace").split()
            if got != ["first", '"quoted', 'arg"', "third"]:
                return f"cmd.exe printed {result.stdout!r}, exit {result.returncode}"

        case "spawn-many":
            count = int(args[0])
            start = time.perf_counter()
            for _ in range(count):
                subprocess.run(
                    [sys.executable, "-S", "-c", ""], check=True, timeout=300
                )
            per = (time.perf_counter() - start) / count * 1000
            print(f"spawn-many: {per:.1f} ms per process over {count}", flush=True)

        case "modify":
            result = child("argv", "modify-me")
            got = json.loads(result.stdout or b"null")
            if got != ["modify-me", "added-by-script"]:
                return (
                    f"the script's modify() did not take effect; child received {got!r}"
                )

        case "drop":
            result = child("argv", "drop-me")
            if result.returncode != 0 or result.stdout != b"":
                return f"the script dropped the command, but it ran: exit {result.returncode}, output {result.stdout!r}"

        case "tagged":
            tag, count = args
            for i in range(int(count)):
                result = child("tag", tag, str(i))
                if result.returncode != 0:
                    return (
                        f"child {i} exited with {result.returncode}: {result.stderr!r}"
                    )
                time.sleep(0.05)

        case _:
            return f"unknown scenario {scenario}"
    return None


def concurrent(catter: list[str]) -> str | None:
    count = 20
    sessions = {}
    for tag in ("A", "B"):
        command = [*catter, sys.executable, HERE, "probe", "tagged", tag, str(count)]
        sessions[tag] = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
    outputs = {}
    for tag, process in sessions.items():
        try:
            outputs[tag] = process.communicate(timeout=300)[0].decode(errors="replace")
        except subprocess.TimeoutExpired:
            process.kill()
            return f"session {tag} did not finish"

    problems = []
    for tag, output in outputs.items():
        other = "B" if tag == "A" else "A"
        own = len(re.findall(rf"\btag {tag} \d+", output))
        foreign = len(re.findall(rf"\btag {other} \d+", output))
        if f"PASS tagged {tag}" not in output or own != count or foreign != 0:
            problems.append(
                f"session {tag} saw {own}/{count} of its commands and {foreign} of "
                f"session {other}'s; output:\n{output[-3000:]}"
            )
    return "\n".join(problems) or None


def report(scenario: str, failure: str | None) -> None:
    if failure is None:
        print(f"PASS {scenario}", flush=True)
    else:
        print(f"FAIL {scenario}: {failure}", flush=True)


def timeout(seconds: float, command: list[str]) -> int:
    # Its own process group, so the whole tree can go: a leftover child
    # holding the output pipe would keep the test waiting.
    options = {} if WINDOWS else {"start_new_session": True}
    process = subprocess.Popen(command, **options)
    try:
        return process.wait(timeout=seconds)
    except subprocess.TimeoutExpired:
        if WINDOWS:
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(process.pid)])
        else:
            os.killpg(process.pid, 9)
        process.wait()
        print(f"TIMEOUT after {seconds}s: {command}", flush=True)
        return 124


def timed(label: str, command: list[str]) -> int:
    start = time.perf_counter()
    code = subprocess.run(command).returncode
    print(f"{label}: {time.perf_counter() - start:.2f} s", flush=True)
    return code


def main() -> None:
    # Failure details quote non-ASCII input, which a Windows console encoding
    # cannot print.
    sys.stdout.reconfigure(errors="backslashreplace")
    match sys.argv[1:]:
        case ["prepare", project, dest]:
            prepare(project, dest)
        case ["generate", count, dest]:
            generate(int(count), dest)
        case ["summarize", "--brief", database, root]:
            summarize(database, root, brief=True)
        case ["summarize", database, root]:
            summarize(database, root, brief=False)
        case ["probe", scenario, *args]:
            report(scenario, probe(scenario, args))
        case ["child", mode, *args]:
            run_child(mode, args)
        case ["concurrent", "--", *catter]:
            report("concurrent", concurrent(catter))
        case ["absent", path]:
            report("absent", f"{path} exists" if os.path.exists(path) else None)
        case ["time", label, "--", *command]:
            sys.exit(timed(label, command))
        case ["timeout", seconds, "--", *command]:
            sys.exit(timeout(float(seconds), command))
        case _:
            sys.exit(__doc__)


if __name__ == "__main__":
    main()
