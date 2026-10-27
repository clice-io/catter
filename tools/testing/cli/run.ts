/// Running catter, and the build tools a test prepares with, in a case's
/// copy, and the transcript a run leaves, normalized so it reads the same
/// on every host: the copy is `${WS}`, catter's data directory
/// `${CATTER_HOME}`, catter's own directory `${CATTER}`, the pixi
/// environment `${ENV}`, durations are elided.

import { spawnSync } from "node:child_process";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { pathToFileURL } from "node:url";
import { REPO_ROOT } from "../../repo.ts";
import { runProcess, type ProcessResult } from "../process.ts";
import type { Table } from "../render/value.ts";

let executable: string | undefined;

/// The catter the tests run: CATTER_EXECUTABLE, with `.exe` supplied on
/// Windows. The hook libraries, and on Linux and macOS catter-proxy, sit
/// beside it.
export function catterExecutable(): string {
    executable ??= locateCatter();
    return executable;
}

function locateCatter(): string {
    let exe = process.env["CATTER_EXECUTABLE"];
    if (!exe) {
        throw new Error("CATTER_EXECUTABLE is not set; point it at the catter a build made");
    }
    if (process.platform === "win32" && !exe.toLowerCase().endsWith(".exe")) {
        const withSuffix = `${exe}.exe`;
        if (fs.existsSync(withSuffix) || !fs.existsSync(exe)) {
            exe = withSuffix;
        }
    }
    if (!fs.existsSync(exe)) {
        throw new Error(`catter executable not found at '${exe}'`);
    }
    return fs.realpathSync.native(path.resolve(exe));
}

/// A run's bound: a build that takes longer is hung.
const COMMAND_TIMEOUT_MS = 240_000;

/// Variables of the runner's own environment that would change how the
/// build tools behave or print.
const DROPPED = [
    "MAKEFLAGS",
    "MFLAGS",
    "MAKELEVEL",
    "CMAKE_GENERATOR",
    "CMAKE_GENERATOR_INSTANCE",
    "CMAKE_GENERATOR_PLATFORM",
    "CMAKE_GENERATOR_TOOLSET",
    "CMAKE_BUILD_PARALLEL_LEVEL",
    "FORCE_COLOR",
    "CLICOLOR_FORCE",
    "NINJA_STATUS",
    // Git Bash's: xmake takes the host for MSYS and builds with MinGW.
    "MSYSTEM",
];

/// The environment of a Visual Studio developer prompt for x64, which
/// builds that call cl.exe themselves (CMake with Ninja) need: the
/// compiler, the SDK tools (rc.exe, mt.exe) and their paths, which only
/// vcvars sets up together. vcvars refuses to run in a prompt that is set
/// up already: it runs in a minimal environment, and what it adds is kept.
function msvcEnvironment(base: NodeJS.ProcessEnv): NodeJS.ProcessEnv {
    const programFiles = base["ProgramFiles(x86)"] ?? "C:\\Program Files (x86)";
    const vswhere = path.join(programFiles, "Microsoft Visual Studio", "Installer", "vswhere.exe");
    const installation = spawnSync(
        vswhere,
        [
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ],
        { encoding: "utf8" },
    ).stdout.trim();
    if (!installation) {
        throw new Error(`no Visual Studio with the C++ tools found by ${vswhere}`);
    }
    const vcvars = path.join(installation, "VC", "Auxiliary", "Build", "vcvars64.bat");
    const kept = [
        "SystemRoot",
        "SystemDrive",
        "windir",
        "ComSpec",
        "PATHEXT",
        "TEMP",
        "TMP",
        "USERPROFILE",
        "USERNAME",
        "APPDATA",
        "LOCALAPPDATA",
        "ProgramData",
        "ProgramFiles",
        "ProgramFiles(x86)",
        "ProgramW6432",
        "CommonProgramFiles",
        "CommonProgramFiles(x86)",
        "PROCESSOR_ARCHITECTURE",
        "NUMBER_OF_PROCESSORS",
    ];
    const clean: NodeJS.ProcessEnv = {};
    for (const name of kept) {
        if (base[name] !== undefined) {
            clean[name] = base[name];
        }
    }
    const root = base["SystemRoot"] ?? "C:\\Windows";
    const system = [
        path.join(root, "System32"),
        root,
        path.join(root, "System32", "Wbem"),
        path.join(root, "System32", "WindowsPowerShell", "v1.0"),
    ];
    clean["PATH"] = system.join(path.delimiter);
    const marker = "::catter-test-environment::";
    const result = spawnSync(
        "cmd.exe",
        ["/d", "/s", "/c", `""${vcvars}" && echo ${marker} && set"`],
        { encoding: "utf8", env: clean, windowsVerbatimArguments: true },
    );
    const [messages, variables] = result.stdout.split(marker);
    if (variables === undefined) {
        throw new Error(
            `${vcvars} failed with ${String(result.status)}:\n${messages ?? ""}\n${result.stderr}`,
        );
    }
    const out: NodeJS.ProcessEnv = {};
    for (const line of variables.split(/\r?\n/)) {
        const at = line.indexOf("=");
        if (at > 0) {
            out[line.slice(0, at)] = line.slice(at + 1);
        }
    }
    const pathKey = Object.keys(out).find((name) => name.toUpperCase() === "PATH") ?? "PATH";
    const added = (out[pathKey] ?? "")
        .split(path.delimiter)
        .filter((entry) => entry !== "" && !system.includes(entry));
    return {
        ...Object.fromEntries(Object.entries(out).filter(([name]) => name !== pathKey)),
        PATH: [...added, base["PATH"] ?? base["Path"] ?? ""].join(path.delimiter),
    };
}

let base: NodeJS.ProcessEnv | undefined;

/// The environment every command of a test runs in, but for catter's data
/// directory: the runner's own, the developer prompt on Windows, and what a
/// handler under test needs: the catter it may start
/// (CATTER_EXECUTABLE) and the Python SDK (`import catter`), in a Python
/// that writes UTF-8, as catter does, and not Windows' ANSI code page, which
/// cannot spell the copy's root, and no bytecode beside the SDK; on macOS,
/// malloc without its nano zone.
export function testEnvironment(): NodeJS.ProcessEnv {
    if (base !== undefined) {
        return base;
    }
    // Windows spells PATH as it likes (`Path`); one spelling of it only.
    let env: NodeJS.ProcessEnv = Object.fromEntries(
        Object.entries(process.env)
            .filter(([name]) => !DROPPED.includes(name))
            .map(([name, value]) => [
                process.platform === "win32" && name.toUpperCase() === "PATH" ? "PATH" : name,
                value,
            ]),
    );
    if (process.platform === "win32") {
        env = { ...env, ...msvcEnvironment(env) };
    }
    env["CATTER_EXECUTABLE"] = catterExecutable();
    env["PYTHONPATH"] = [path.join(REPO_ROOT, "sdk", "python"), env["PYTHONPATH"]]
        .filter((entry) => entry !== undefined && entry !== "")
        .join(path.delimiter);
    env["PYTHONUTF8"] = "1";
    env["PYTHONDONTWRITEBYTECODE"] = "1";
    if (process.platform === "darwin") {
        // ASan, in catter's Debug build, takes the address space of malloc's
        // nano zone, and malloc says so before main: on a terminal, else
        // through a socket to the system log, which takes the number of a
        // standard stream the program was started without.
        env["MallocNanoZone"] = "0";
    }
    base = env;
    return env;
}

function posixOf(rel: string): string {
    return rel.split(path.sep).join("/");
}

/// Every spelling of `dir` an output may use: as given, resolved, with
/// forward slashes, each with a lowercase drive, JSON-escaped.
function spellings(dir: string): string[] {
    let forms = spelled.get(dir);
    if (forms === undefined) {
        forms = findSpellings(dir);
        spelled.set(dir, forms);
    }
    return forms;
}

const spelled = new Map<string, string[]>();

/// The 8.3 name of an existing path on Windows, as CMake writes a
/// compiler's path that holds spaces: `C:\PROGRA~1\MICROS~2\…\cl.exe`.
function shortPath(dir: string): string | undefined {
    if (process.platform !== "win32" || !fs.existsSync(dir)) {
        return undefined;
    }
    const result = spawnSync("cmd.exe", ["/d", "/c", `for %I in ("${dir}") do @echo %~sI`], {
        encoding: "utf8",
        windowsVerbatimArguments: true,
    });
    const short = result.stdout.trim();
    return short === "" ? undefined : short;
}

function findSpellings(dir: string): string[] {
    const forms = new Set<string>();
    const resolved = fs.existsSync(dir) ? fs.realpathSync.native(dir) : dir;
    const short = shortPath(resolved.replace(/[\\/]+$/, ""));
    for (const form of [dir, resolved, ...(short === undefined ? [] : [short])]) {
        const bare = form.replace(/[\\/]+$/, "");
        for (const spelled of [bare, bare.split(path.sep).join("/")]) {
            forms.add(spelled);
            forms.add(spelled.replace(/^[A-Z]:/, (drive) => drive.toLowerCase()));
            forms.add(spelled.replace(/^[a-z]:/, (drive) => drive.toUpperCase()));
            // Windows names are case-insensitive, and xmake writes them lowercase.
            if (process.platform === "win32") {
                forms.add(spelled.toLowerCase());
            }
        }
        forms.add(JSON.stringify(bare).slice(1, -1));
        // A file URL's path, percent-encoded, as node's stack traces print it.
        forms.add(pathToFileURL(bare).pathname);
    }
    return [...forms].filter((form) => form.length > 1);
}

/// The directories a test's output names by placeholder, the copy first.
export interface Places {
    /// The copy's root, `${WS}`.
    root: string;
    /// catter's data directory, `${CATTER_HOME}`.
    home: string;
}

/// The developer directory of the Xcode the build tools use on macOS.
let xcode: string | undefined;

let outer: [string, string][] | undefined;

/// Directories outside the copy an output may name, by placeholder.
function outerPlaces(): [string, string][] {
    outer ??= findOuterPlaces();
    return outer;
}

function findOuterPlaces(): [string, string][] {
    const out: [string, string][] = [
        ["${CATTER}", path.dirname(catterExecutable())],
        ["${TMP}", os.tmpdir()],
    ];
    const env = testEnvironment();
    const prefix = env["CONDA_PREFIX"];
    if (prefix !== undefined && prefix !== "") {
        out.push(["${ENV}", prefix]);
    }
    for (const [name, variable] of [
        ["${MSVC}", "VCToolsInstallDir"],
        ["${WINSDK}", "WindowsSdkDir"],
    ] as const) {
        const dir = env[variable];
        if (dir !== undefined && dir !== "") {
            out.push([name, dir]);
        }
    }
    if (process.platform === "darwin") {
        xcode ??= spawnSync("xcode-select", ["-p"], {
            encoding: "utf8",
        }).stdout.trim();
        out.push(["${XCODE}", xcode]);
        // Where Xcode keeps the caches of the user, as its SDK stat caches.
        const caches = spawnSync("getconf", ["DARWIN_USER_CACHE_DIR"], {
            encoding: "utf8",
        }).stdout.trim();
        out.push(["${DARWIN_CACHE}", caches]);
    }
    return out;
}

/// The copy's files and directories in their native spelling, longest
/// first: on Windows a path under the root may hold spaces, which no
/// pattern over the text can tell from the words around it.
function nativePaths(root: string): string[] {
    if (path.sep === "/" || !fs.existsSync(root)) {
        return [];
    }
    return fs
        .readdirSync(root, { recursive: true, encoding: "utf8" })
        .filter((rel) => rel.includes(path.sep))
        .sort((a, b) => b.length - a.length);
}

const ANSI = /\u001b\[[0-9;]*[A-Za-z]/g;

/// `text` as it reads on every host: the places it names by placeholder
/// with forward slashes, line ends LF, colors dropped.
export function portable(text: string, places: Places): string {
    let out = text.replace(/\r+\n/g, "\n").replace(ANSI, "");
    const named: [string, string][] = [
        ["${WS}", places.root],
        ["${CATTER_HOME}", places.home],
        ...outerPlaces(),
    ];
    const forms = named
        .flatMap(([name, dir]) => spellings(dir).map((form) => [name, form] as const))
        .sort(([, a], [, b]) => b.length - a.length);
    const rels = nativePaths(places.root);
    for (const [name, form] of forms) {
        if (name === "${WS}") {
            const sep = form.includes("\\\\") ? "\\\\" : path.sep;
            for (const rel of rels) {
                out = out.replaceAll(
                    `${form}${sep}${rel.split(path.sep).join(sep)}`,
                    `\${WS}/${posixOf(rel)}`,
                );
            }
        }
        out = out.replaceAll(form, name);
    }
    return out.replace(/(\$\{[A-Z_]+\}[^\s"']*)/g, (span) =>
        span.replaceAll("\\\\", "/").replaceAll("\\", "/"),
    );
}

/// The frames of a native stack trace (cpptrace's), whose addresses every
/// build and every run changes.
const NATIVE_FRAMES = /(?:^#\d+ +0x[0-9a-f]+ .*(?:\n|$))+/gm;

/// The directory of the temporary files xmake compiles to probe the
/// compiler, named by the user (but on Windows) and the day, and the files,
/// by a hash: `${TMP}/.xmake1001/261024/_2a7e…f3.cpp`.
const XMAKE_PROBE_DIR = /\$\{TMP\}\/\.xmake\d*\/\d{6}/g;
const XMAKE_PROBE = /\$\{TMP\}\/\.xmake#\/#\/_[0-9a-f]{32}/g;

/// The version of the macOS SDK and of the deployment target, which every
/// update of the runner's Xcode changes: `MacOSX15.5.sdk`,
/// `arm64-apple-macos15.5`.
const MACOS_VERSION = /\b(MacOSX|apple-macos)\d+(?:\.\d+)*/g;

/// Xcode's stat cache of an SDK, named by the versions of Xcode and the SDK:
/// `${DARWIN_CACHE}/com.apple.DeveloperTools/16.4-16F6/Xcode/SDKStatCaches.noindex/macosx15.5-24F74-a403….sdkstatcache`.
const SDK_STAT_CACHE =
    /(\$\{DARWIN_CACHE\}\/com\.apple\.DeveloperTools\/)[^/]+(\/Xcode\/SDKStatCaches\.noindex\/)[^/\s"']+(\.sdkstatcache)/g;

/// The pipe a session listens on, named by catter's process id:
/// `\\.\pipe\catter-ipc-9420`, `${CATTER_HOME}/ipc-9420.sock`.
const SESSION_PIPE = /\b((?:catter-)?ipc-)\d+\b/g;

/// A command's output as it reads on every host and every run: portable,
/// native stack frames, process ids, durations, the macOS SDK's version
/// and build tools' temporary names elided, every line's end trimmed.
export function normalize(text: string, places: Places): string {
    return portable(text, places)
        .replace(NATIVE_FRAMES, "#… native frames\n")
        .replace(SESSION_PIPE, "$1#")
        .replace(SDK_STAT_CACHE, "$1#$2#$3")
        .replace(MACOS_VERSION, "$1#")
        .replace(XMAKE_PROBE_DIR, "${TMP}/.xmake#/#")
        .replace(XMAKE_PROBE, "${TMP}/.xmake#/#/_#")
        .replace(/\b\d+(\.\d+)?(ms|s)\b/g, "#$2")
        .split("\n")
        .map((line) => line.trimEnd())
        .join("\n")
        .replace(/\n+$/, "");
}

/// Split a command line as a POSIX shell does for plain words and double
/// or single quotes; nothing else is shell syntax here.
export function splitCommand(line: string): string[] {
    const words: string[] = [];
    let word = "";
    let quote: string | null = null;
    let started = false;
    for (const char of line) {
        if (quote !== null) {
            if (char === quote) {
                quote = null;
            } else {
                word += char;
            }
        } else if (char === '"' || char === "'") {
            quote = char;
            started = true;
        } else if (char === " ") {
            if (started) {
                words.push(word);
            }
            word = "";
            started = false;
        } else {
            word += char;
            started = true;
        }
    }
    if (quote !== null) {
        throw new Error(`unterminated quote in: ${line}`);
    }
    if (started) {
        words.push(word);
    }
    return words;
}

function quote(word: string): string {
    return /^[\p{L}\p{N}_./:=@%+,${}-]+$/u.test(word) ? word : `"${word.replaceAll('"', '\\"')}"`;
}

/// The label of a command's block: `$ <program> <args>`, normalized.
export function commandLabel(program: string, args: readonly string[], places: Places): string {
    return ["$", program, ...args.map((arg) => quote(normalize(arg, places)))].join(" ");
}

export interface CommandOptions {
    env?: Record<string, string>;
    /// The working directory, relative to the copy; its root by default.
    cwd?: string;
    /// What the run reads on stdin; nothing when unset.
    input?: string;
}

/// What a command printed and how it ended, normalized: stdout, stderr,
/// and the exit status or the signal that killed it.
export function transcript(result: ProcessResult, places: Places): Table {
    const stdout = normalize(result.stdout, places);
    const stderr = normalize(result.stderr, places);
    return {
        ...(stdout === "" ? {} : { stdout }),
        ...(stderr === "" ? {} : { stderr }),
        ...(result.status === null ? { killed: String(result.signal) } : { exit: result.status }),
    };
}

/// Run `program` in the copy at `places.root` with the test's environment
/// and catter's data directory, until it exits or is hung.
export async function runCommand(
    program: string,
    args: readonly string[],
    places: Places,
    options: CommandOptions = {},
): Promise<ProcessResult> {
    const result = await runProcess(program, args, {
        cwd: path.join(places.root, options.cwd ?? "."),
        env: { ...testEnvironment(), CATTER_HOME: places.home, ...options.env },
        timeout: COMMAND_TIMEOUT_MS,
        ...(options.input === undefined ? {} : { input: options.input }),
    });
    if (result.error !== undefined) {
        throw new Error(
            `${program} ${args.join(" ")}: ${result.error.message}\nstdout:\n${result.stdout}\nstderr:\n${result.stderr}`,
        );
    }
    return result;
}
