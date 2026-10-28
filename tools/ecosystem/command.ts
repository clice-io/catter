/// What the checks know of a command line without catter: whether a program
/// is a compiler or a tool catter's wrapper stands in for, by its name, and
/// the arguments a response file holds. Kept apart from catter's own tables
/// on purpose: the checks are its reference.

import * as fs from "node:fs";
import * as path from "node:path";

const COMPILER = /^(?:.*-)?(?:gcc|g\+\+|clang|clang\+\+|cc|c\+\+|cl|clang-cl|nvcc)(?:-[\d.]+)?(?:\.exe)?$/i;
const LINKER_OR_ARCHIVER = /^(?:.*-)?(?:ar|ranlib|ld|ld\.bfd|ld\.gold|ld\.lld|lld|lld-link|llvm-ar|llvm-lib|lib|link)(?:-[\d.]+)?(?:\.exe)?$/i;
const SOURCE = /\.(?:c|cc|cpp|cxx|c\+\+|m|mm|cu|s|S|sx)$/;

export function baseName(program: string): string {
    return program.split(/[\\/]/).pop() ?? program;
}

export function isCompiler(program: string): boolean {
    return COMPILER.test(baseName(program));
}

/// A program catter's wrapper backend stands in for: a compiler, a linker
/// or an archiver, by its name.
export function isWrapped(program: string): boolean {
    return isCompiler(program) || LINKER_OR_ARCHIVER.test(baseName(program));
}

/// Options that only write what a source depends on (`-MD -MF a.d`):
/// they change no compile.
const DEPENDENCY_FILES = new Set(["-MD", "-MMD", "-MP", "-M", "-MM"]);
const DEPENDENCY_FILES_WITH_VALUE = new Set(["-MF", "-MT", "-MQ"]);

/// The arguments without the options that only write dependency files, and
/// without `dropped`, options taking a value, joined or not.
export function withoutDependencyFiles(args: readonly string[], dropped: readonly string[] = []): string[] {
    const withValue = [...DEPENDENCY_FILES_WITH_VALUE, ...dropped];
    const out: string[] = [];
    for (let i = 0; i < args.length; i += 1) {
        const arg = args[i] ?? "";
        if (DEPENDENCY_FILES.has(arg) || arg.startsWith("-Wp,-MD,") || arg.startsWith("-Wp,-MMD,")) {
            continue;
        }
        if (withValue.includes(arg)) {
            i += 1;
            continue;
        }
        if (withValue.some((option) => arg.startsWith(option) && arg.length > option.length)) {
            continue;
        }
        out.push(arg);
    }
    return out;
}

/// Whether `arg` names a source file that is there now, from `cwd`.
export function isSource(arg: string, cwd: string): boolean {
    return !arg.startsWith("-") && SOURCE.test(arg) && fs.statSync(path.resolve(cwd, arg), { throwIfNoEntry: false })?.isFile() === true;
}

/// Split a command line as a POSIX shell splits words: quotes, and a
/// backslash outside single quotes, as CMake and Meson write a compilation
/// database's `command`.
export function shellSplit(line: string): string[] {
    const words: string[] = [];
    let word = "";
    let started = false;
    let quote: "'" | '"' | null = null;
    for (let i = 0; i < line.length; i += 1) {
        const char = line[i] ?? "";
        if (quote === "'") {
            if (char === "'") {
                quote = null;
            } else {
                word += char;
            }
        } else if (char === "\\" && i + 1 < line.length) {
            const next = line[i + 1] ?? "";
            if (quote === '"' && !['"', "\\", "$", "`"].includes(next)) {
                word += char;
            } else {
                word += next;
                i += 1;
            }
            started = true;
        } else if (quote === '"') {
            if (char === '"') {
                quote = null;
            } else {
                word += char;
            }
        } else if (char === "'" || char === '"') {
            quote = char;
            started = true;
        } else if (/\s/.test(char)) {
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
    if (started) {
        words.push(word);
    }
    return words;
}

/// The arguments with every response file (`@file`) there is from `cwd`
/// read in, as GCC and Clang read them.
export function expandResponseFiles(args: readonly string[], cwd: string, depth = 0): string[] {
    return args.flatMap((arg) => {
        if (!arg.startsWith("@") || depth > 4) {
            return [arg];
        }
        const file = path.resolve(cwd, arg.slice(1));
        if (!fs.statSync(file, { throwIfNoEntry: false })?.isFile()) {
            return [arg];
        }
        return expandResponseFiles(shellSplit(fs.readFileSync(file, "utf8")), cwd, depth + 1);
    });
}
