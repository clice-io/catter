#!/usr/bin/env node

import { accessSync } from "node:fs";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const FLAG_BITS = {
  HelpHidden: 1 << 0,
  RenderAsInput: 1 << 1,
  RenderJoined: 1 << 2,
  Ignored: 1 << 4,
  LinkOption: 1 << 5,
  LinkerInput: 1 << 6,
  NoArgumentUnused: 1 << 7,
  NoXarchOption: 1 << 8,
  TargetSpecific: 1 << 9,
  Unsupported: 1 << 10,
};

const VISIBILITY_BITS = {
  DefaultVis: 1 << 0,
  CLOption: 1 << 1,
  CC1Option: 1 << 2,
  CC1AsOption: 1 << 3,
  FC1Option: 1 << 4,
  DXCOption: 1 << 5,
  FlangOption: 1 << 6,
};

const TABLES = [
  {
    key: "clang",
    input: "clang-Driver-Options.inc",
    output: "clang.ts",
    prefix: "Clang",
    aliases: ["ClangDriverClass"],
  },
  {
    key: "lld-coff",
    input: "lld-COFF-Options.inc",
    output: "lld-coff.ts",
    prefix: "LldCoff",
  },
  {
    key: "lld-elf",
    input: "lld-ELF-Options.inc",
    output: "lld-elf.ts",
    prefix: "LldElf",
  },
  {
    key: "lld-macho",
    input: "lld-MachO-Options.inc",
    output: "lld-macho.ts",
    prefix: "LldMachO",
  },
  {
    key: "lld-mingw",
    input: "lld-MinGW-Options.inc",
    output: "lld-mingw.ts",
    prefix: "LldMinGW",
  },
  {
    key: "lld-wasm",
    input: "lld-wasm-Options.inc",
    output: "lld-wasm.ts",
    prefix: "LldWasm",
  },
  {
    key: "llvm-dlltool",
    input: "llvm-dlltool-Options.inc",
    output: "llvm-dlltool.ts",
    prefix: "LlvmDlltool",
  },
  {
    key: "llvm-lib",
    input: "llvm-lib-Options.inc",
    output: "llvm-lib.ts",
    prefix: "LlvmLib",
  },
];

const EXTRA_EXPORT_TABLES = [
  {
    output: "nvcc.ts",
    prefix: "Nvcc",
  },
];

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const outputDir = path.join(currentDir, "..", "..", "api", "src", "option");

async function main() {
  const selectedTables = resolveTables(process.argv.slice(2));
  const condaPrefix = resolveCondaPrefix();

  await mkdir(outputDir, { recursive: true });

  for (const table of selectedTables) {
    const inputFile = path.join(
      condaPrefix,
      "include",
      "llvm-options-td",
      table.input,
    );
    const source = await readFile(inputFile, "utf8");
    const entries = extractOptionEntries(source).map(parseOptionEntry);
    const outputFile = path.join(outputDir, table.output);
    const rendered = renderModule(table, inputFile, entries);

    await writeFile(outputFile, rendered);
    process.stdout.write(
      `generated ${path.relative(process.cwd(), outputFile)} from ${inputFile}\n`,
    );
  }

  const indexFile = path.join(outputDir, "index.ts");
  await writeFile(indexFile, renderIndexModule([...TABLES, ...EXTRA_EXPORT_TABLES]));
  process.stdout.write(`generated ${path.relative(process.cwd(), indexFile)}\n`);
}

function resolveTables(args) {
  args = args.filter((arg) => arg !== "--");

  if (args.includes("-h") || args.includes("--help")) {
    printHelp();
    process.exit(0);
  }

  if (args.length === 0) {
    return TABLES;
  }

  const byKey = new Map(TABLES.map((table) => [table.key, table]));
  const selected = [];
  for (const key of args) {
    const table = byKey.get(key);
    if (!table) {
      throw new Error(
        `unknown table ${JSON.stringify(key)}. available tables: ${TABLES.map((item) => item.key).join(", ")}`,
      );
    }
    selected.push(table);
  }
  return selected;
}

function printHelp() {
  const script = "scripts/js-gen/generate_llvm_option.mjs";
  process.stdout.write(`Usage:
  node ${script} [table...]

Examples:
  node ${script}
  node ${script} clang
  node ${script} lld-coff lld-elf lld-macho

Available tables:
  ${TABLES.map((table) => table.key).join("\n  ")}
`);
}

function resolveCondaPrefix() {
  if (process.env.CONDA_PREFIX) {
    return process.env.CONDA_PREFIX;
  }

  const projectRoot = path.join(currentDir, "..", "..");
  const pixiPrefixes = [
    path.join(projectRoot, ".pixi", "envs", "dev"),
    path.join(projectRoot, ".pixi", "envs", "default"),
  ];

  for (const prefix of pixiPrefixes) {
    try {
      const includeDir = path.join(prefix, "include", "llvm-options-td");
      accessSync(path.join(includeDir, "clang-Driver-Options.inc"));
      return prefix;
    } catch {
      // keep trying
    }
  }

  throw new Error("CONDA_PREFIX is not set and no .pixi env with llvm option headers was found");
}

function extractOptionEntries(source) {
  const sectionMatch = source.match(/#ifdef OPTION([\s\S]*?)#endif \/\/ OPTION/);
  if (!sectionMatch) {
    throw new Error("failed to locate OPTION section");
  }

  const section = sectionMatch[1];
  const entries = [];
  let cursor = 0;

  while (true) {
    const optionIndex = section.indexOf("OPTION(", cursor);
    if (optionIndex === -1) {
      break;
    }

    const bodyStart = optionIndex + "OPTION(".length;
    let depth = 1;
    let current = bodyStart;
    let quote = null;

    while (current < section.length) {
      const char = section[current];
      if (quote) {
        if (char === "\\") {
          current += 2;
          continue;
        }
        if (char === quote) {
          quote = null;
        }
      } else if (char === '"' || char === "'") {
        quote = char;
      } else if (char === "(") {
        depth += 1;
      } else if (char === ")") {
        depth -= 1;
        if (depth === 0) {
          break;
        }
      }
      current += 1;
    }

    if (depth !== 0) {
      throw new Error("unterminated OPTION(...) entry");
    }

    entries.push(section.slice(bodyStart, current));
    cursor = current + 1;
  }

  return entries;
}

function parseOptionEntry(entry) {
  const fields = splitTopLevel(stripComments(entry));
  if (fields.length < 9) {
    throw new Error(`expected at least 9 OPTION fields, got ${fields.length}`);
  }

  return {
    id: fields[2],
    flags: fields[7],
    visibility: fields[8],
  };
}

function stripComments(text) {
  let result = "";
  let cursor = 0;
  let quote = null;

  while (cursor < text.length) {
    const char = text[cursor];

    if (quote) {
      result += char;
      if (char === "\\") {
        result += text[cursor + 1] ?? "";
        cursor += 2;
        continue;
      }
      if (char === quote) {
        quote = null;
      }
      cursor += 1;
      continue;
    }

    if (char === '"' || char === "'") {
      quote = char;
      result += char;
      cursor += 1;
      continue;
    }

    if (char === "/" && text[cursor + 1] === "/") {
      cursor += 2;
      while (cursor < text.length && text[cursor] !== "\n") {
        cursor += 1;
      }
      continue;
    }

    if (char === "/" && text[cursor + 1] === "*") {
      cursor += 2;
      while (cursor + 1 < text.length &&
             !(text[cursor] === "*" && text[cursor + 1] === "/")) {
        cursor += 1;
      }
      cursor += 2;
      continue;
    }

    result += char;
    cursor += 1;
  }

  return result;
}

function splitTopLevel(text) {
  const parts = [];
  let current = "";
  let parenDepth = 0;
  let braceDepth = 0;
  let bracketDepth = 0;
  let angleDepth = 0;
  let quote = null;

  for (let i = 0; i < text.length; i += 1) {
    const char = text[i];

    if (quote) {
      current += char;
      if (char === "\\") {
        current += text[i + 1] ?? "";
        i += 1;
        continue;
      }
      if (char === quote) {
        quote = null;
      }
      continue;
    }

    if (char === '"' || char === "'") {
      quote = char;
      current += char;
      continue;
    }

    if (char === "(") {
      parenDepth += 1;
    } else if (char === ")") {
      parenDepth -= 1;
    } else if (char === "{") {
      braceDepth += 1;
    } else if (char === "}") {
      braceDepth -= 1;
    } else if (char === "[") {
      bracketDepth += 1;
    } else if (char === "]") {
      bracketDepth -= 1;
    } else if (char === "<") {
      angleDepth += 1;
    } else if (char === ">") {
      angleDepth = Math.max(0, angleDepth - 1);
    }

    if (char === "," &&
        parenDepth === 0 &&
        braceDepth === 0 &&
        bracketDepth === 0 &&
        angleDepth === 0) {
      parts.push(current.trim());
      current = "";
      continue;
    }

    current += char;
  }

  if (current.trim()) {
    parts.push(current.trim());
  }

  return parts;
}

function renderModule(table, inputFile, entries) {
  const ids = Array.from(new Set(entries.map((entry) => entry.id)));
  const flags = collectBitMembers(entries.map((entry) => entry.flags), FLAG_BITS);
  const visibility = collectBitMembers(
    entries.map((entry) => entry.visibility),
    VISIBILITY_BITS,
  );

  const sections = [
    renderEnum(`${table.prefix}ID`, [["ID_INVALID", 0], ...ids.map((id) => [`ID_${id}`, null])]),
    renderEnum(`${table.prefix}Flag`, flags),
    renderEnum(`${table.prefix}Visibility`, visibility),
  ];

  for (const aliasName of table.aliases ?? []) {
    sections.push(renderEnum(aliasName, visibility));
  }

  return `// Generated by scripts/js-gen/generate_llvm_option.mjs.
// Source: ${inputFile}

${sections.join("\n\n")}
`;
}

function collectBitMembers(expressions, knownBits) {
  const members = [["None", 0]];
  const used = new Set();

  for (const expression of expressions) {
    for (const name of expression.match(/[A-Za-z_][A-Za-z0-9_]*/g) ?? []) {
      if (Object.hasOwn(knownBits, name)) {
        used.add(name);
      }
    }
  }

  for (const name of Object.keys(knownBits)) {
    if (used.has(name)) {
      members.push([name, knownBits[name]]);
    }
  }

  return members;
}

function renderEnum(enumName, members) {
  const body = members
    .map(([name, value]) => {
      if (value === null) {
        return `  ${name},`;
      }
      return `  ${name} = ${value},`;
    })
    .join("\n");

  return `export enum ${enumName} {
${body}
}`;
}

function renderIndexModule(tables) {
  const exports = [];

  for (const table of tables) {
    const base = path.basename(table.output, ".ts");
    exports.push(`export { ${table.prefix}ID } from "./${base}.js";`);
    exports.push(`export { ${table.prefix}Flag } from "./${base}.js";`);
    exports.push(`export { ${table.prefix}Visibility } from "./${base}.js";`);

    for (const aliasName of table.aliases ?? []) {
      exports.push(`export { ${aliasName} } from "./${base}.js";`);
    }
  }

  return `// Generated by scripts/js-gen/generate_llvm_option.mjs.

import { option_get_info, option_parse } from "catter-c";
import type { OptionInfo, OptionItem, OptionTable } from "catter-c";

${exports.join("\n")}

const RENDER_JOINED = 1 << 2;
const RENDER_SEPARATE = 1 << 3;
const OptionKindClass: {
    GroupClass: number;
    InputClass: number;
    UnknownClass: number;
    FlagClass: number;
    JoinedClass: number;
    ValuesClass: number;
    SeparateClass: number;
    RemainingArgsClass: number;
    RemainingArgsJoinedClass: number;
    CommaJoinedClass: number;
    MultiArgClass: number;
    JoinedOrSeparateClass: number;
    JoinedAndSeparateClass: number;
} = {
    GroupClass: 0,
    InputClass: 1,
    UnknownClass: 2,
    FlagClass: 3,
    JoinedClass: 4,
    ValuesClass: 5,
    SeparateClass: 6,
    RemainingArgsClass: 7,
    RemainingArgsJoinedClass: 8,
    CommaJoinedClass: 9,
    MultiArgClass: 10,
    JoinedOrSeparateClass: 11,
    JoinedAndSeparateClass: 12,
};

function cloneOptionItem(item: OptionItem): OptionItem {
    return {
        ...item,
        values: [...item.values],
    };
}

function joinedTokens(key: string, values: string[]): string[] {
    if (values.length === 0) {
        return [key];
    }
    return [key + values[0], ...values.slice(1)];
}

function renderTokens(info: OptionInfo, item: OptionItem): string[] {

    if (info.flags & RENDER_JOINED) {
        return joinedTokens(item.key, item.values);
    }
    if (info.flags & RENDER_SEPARATE) {
        return [item.key, ...item.values];
    }

    switch (info.kind) {
        case OptionKindClass.GroupClass:
        case OptionKindClass.InputClass:
        case OptionKindClass.UnknownClass:
            return [item.key];
        case OptionKindClass.FlagClass:
        case OptionKindClass.ValuesClass:
        case OptionKindClass.SeparateClass:
        case OptionKindClass.JoinedOrSeparateClass:
        case OptionKindClass.RemainingArgsClass:
        case OptionKindClass.MultiArgClass:
            return [item.key, ...item.values];
        case OptionKindClass.JoinedClass:
        case OptionKindClass.RemainingArgsJoinedClass:
            return joinedTokens(item.key, item.values);
        case OptionKindClass.CommaJoinedClass:
            return item.values.length === 0
                ? [item.key]
                : [item.key + item.values.join(",")];
        case OptionKindClass.JoinedAndSeparateClass:
            return joinedTokens(item.key, item.values);
        default:
            return [item.key, ...item.values];
    }
}

export function convertToUnalias(
    table: OptionTable,
    item: OptionItem,
): OptionItem {
    if (item.unalias === undefined) {
        return item;
    }

    const aliasInfo = option_get_info(table, item.id);
    const unaliasInfo = option_get_info(table, item.unalias);

    item.id = item.unalias;
    item.unalias = undefined;
    if (
        unaliasInfo.kind !== OptionKindClass.InputClass &&
        unaliasInfo.kind !== OptionKindClass.UnknownClass
    ) {
        item.key = unaliasInfo.prefixedKey;
    }
    item.values.push(...aliasInfo.aliasArgs);
    if (
        aliasInfo.kind === OptionKindClass.FlagClass &&
        aliasInfo.aliasArgs.length === 0 &&
        unaliasInfo.kind === OptionKindClass.JoinedClass
    ) {
        item.values.push("");
    }

    return item;
}

export function stringify(table: OptionTable, item: OptionItem): string {
    const renderItem =
        item.unalias === undefined
            ? item
            : convertToUnalias(table, cloneOptionItem(item));
    const info = option_get_info(table, renderItem.id);
    return renderTokens(info, renderItem).join(" ");
}

export function collect(table: OptionTable, args: string[]): OptionItem[] | string {
    let res: OptionItem[] | string = [];
    option_parse(table, args, parseRes => {
        if (typeof parseRes === "string") {
            res = parseRes;
            return false;
        }
        (res as OptionItem[]).push(parseRes);
        return true;
    });
    return res;
}

export function replace(table: OptionTable, args: string[], cb: (parseRes: string | Readonly<OptionItem>) => OptionItem | boolean | undefined | string | string[]): string {
    let nextToAdd = 0;
    let prevIndex = -1;
    let newPart = "";
    let finalArgs: string[] = [];
    const concatParts = (endIndex: number) => {
        finalArgs = finalArgs.concat([...args.slice(nextToAdd, prevIndex), newPart]);
        nextToAdd = endIndex;
    }
    option_parse(table, args, parseRes => {
        if (prevIndex != -1) {
            concatParts(typeof parseRes === "string" ? args.length : parseRes.index);
            prevIndex = -1;
        }
        const cbRes = cb(parseRes);
        if (cbRes === undefined) {
            return true;
        }
        if (typeof cbRes === "boolean") {
            return cbRes;
        }

        if (typeof cbRes === "string") {
            newPart = cbRes;
        } else if (Array.isArray(cbRes)) {
            newPart = cbRes.join(" ");
        } else {
            newPart = stringify(table, cbRes);
        }

        if (typeof parseRes !== "string") {
            prevIndex = parseRes.index;
        }
        return true;
    });
    if (prevIndex != -1) {
        concatParts(args.length);
    }
    prevIndex = args.length;
    concatParts(-1);
    finalArgs.pop();
    return finalArgs.join(" ");
}
`;
}

main().catch((error) => {
  process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
  process.exitCode = 1;
});
