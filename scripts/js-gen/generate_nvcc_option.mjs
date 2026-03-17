#!/usr/bin/env node

import { execFileSync } from "node:child_process";
import { mkdir, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const FLAG_BITS = {
  HelpHidden: 1 << 0,
  RenderAsInput: 1 << 1,
  RenderJoined: 1 << 2,
  RenderSeparate: 1 << 3,
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
};

const SECTION_ID_NAMES = new Map([
  ["Options for specifying the compilation phase", "Compilation_phase_Group"],
  ["File and path specifications.", "File_path_Group"],
  ["Options for specifying behavior of compiler/linker.", "Compiler_linker_behavior_Group"],
  ["Options for passing specific phase options", "Phase_options_Group"],
  ["Miscellaneous options for guiding the compiler driver.", "Driver_misc_Group"],
  ["Options for steering GPU code generation.", "GPU_code_generation_Group"],
  ["Options for steering cuda compilation.", "Cuda_compilation_Group"],
  ["Generic tool options.", "Generic_tool_Group"],
]);

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.join(currentDir, "..", "..");
const optionOutputDir = path.join(projectRoot, "api", "src", "option");
const externalOutputDir = path.join(projectRoot, "src", "common", "opt", "external");

main().catch((error) => {
  process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
  process.exitCode = 1;
});

async function main() {
  const nvccPath = resolveNvccPath();
  const helpText = execFileSync(nvccPath, ["--help"], { encoding: "utf8" });
  const parsed = parseHelp(helpText);
  const generated = buildGeneratedModel(parsed);

  await mkdir(optionOutputDir, { recursive: true });
  await mkdir(externalOutputDir, { recursive: true });

  const tsFile = path.join(optionOutputDir, "nvcc.ts");
  const headerFile = path.join(externalOutputDir, "nvcc.h");
  const sourceFile = path.join(externalOutputDir, "nvcc.cc");

  await writeFile(tsFile, renderTsModule(generated, nvccPath));
  await writeFile(headerFile, renderHeader(generated, nvccPath));
  await writeFile(sourceFile, renderSource(generated, nvccPath));

  process.stdout.write(`generated ${path.relative(projectRoot, tsFile)} from ${nvccPath} --help\n`);
  process.stdout.write(`generated ${path.relative(projectRoot, headerFile)} from ${nvccPath} --help\n`);
  process.stdout.write(`generated ${path.relative(projectRoot, sourceFile)} from ${nvccPath} --help\n`);
}

function resolveNvccPath() {
  const candidates = [
    process.env.NVCC_PATH,
    process.env.NVCC,
    process.env.CUDA_HOME ? path.join(process.env.CUDA_HOME, "bin", "nvcc") : undefined,
    process.env.CUDA_PATH ? path.join(process.env.CUDA_PATH, "bin", "nvcc") : undefined,
    "/usr/local/cuda/bin/nvcc",
    "/opt/cuda/bin/nvcc",
  ].filter(Boolean);

  for (const candidate of candidates) {
    try {
      execFileSync(candidate, ["--version"], { stdio: "ignore" });
      return candidate;
    } catch {
      // keep trying
    }
  }

  try {
    const found = execFileSync("bash", ["-lc", "command -v nvcc"], { encoding: "utf8" }).trim();
    if (found) {
      return found;
    }
  } catch {
    // ignore
  }

  throw new Error("failed to locate nvcc; set NVCC_PATH or ensure nvcc is installed");
}

function parseHelp(helpText) {
  const lines = helpText.split(/\r?\n/);
  const options = [];
  let currentSection = "General";

  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i];

    if (lines[i + 1] && /^=+$/.test(lines[i + 1])) {
      currentSection = line.trim();
      continue;
    }

    if (!line.startsWith("--")) {
      continue;
    }

    const optionLine = parseOptionLine(line);
    if (!optionLine) {
      throw new Error(`failed to parse nvcc option line: ${JSON.stringify(line)}`);
    }

    const helpLines = [];
    let cursor = i + 1;
    while (cursor < lines.length) {
      const next = lines[cursor];
      if (next.startsWith("--")) {
        break;
      }
      if (lines[cursor + 1] && /^=+$/.test(lines[cursor + 1])) {
        break;
      }
      if (next.trim().length === 0) {
        cursor += 1;
        continue;
      }
      if (!/^\s{2,}/.test(next)) {
        break;
      }
      helpLines.push(next.trim());
      cursor += 1;
    }

    options.push({
      ...optionLine,
      section: currentSection,
      help: helpLines.join(" "),
    });
  }

  return options;
}

function parseOptionLine(line) {
  const aliasMatch = line.match(/\s{2,}(\(-[^)]*\)|--\S+)\s*$/);
  if (!aliasMatch || aliasMatch.index === undefined) {
    return null;
  }

  const left = line.slice(0, aliasMatch.index).trimEnd();
  const leftMatch = left.match(/^--(?<long>[A-Za-z0-9][A-Za-z0-9-]*)(?<value>.*)$/);
  if (!leftMatch?.groups) {
    return null;
  }

  const longName = leftMatch.groups.long;
  const valueSyntax = leftMatch.groups.value.trim();
  let alias = aliasMatch[1];
  if (alias.startsWith("(") && alias.endsWith(")")) {
    alias = alias.slice(1, -1);
  }
  if (alias === `--${longName}` || alias.length === 0) {
    alias = `-${longName}`;
  }

  return {
    longName,
    valueSyntax,
    alias,
  };
}

function buildGeneratedModel(parsedOptions) {
  const idAllocator = createIdAllocator();
  const groupIds = new Map();
  const groupEntries = [];

  for (const option of parsedOptions) {
    if (!groupIds.has(option.section)) {
      const groupBase = SECTION_ID_NAMES.get(option.section) ?? `${sanitizeIdentifier(option.section)}_Group`;
      const idName = idAllocator(`ID_${groupBase}`);
      groupIds.set(option.section, idName);
      groupEntries.push({
        idName,
        kind: "Group",
        prefixExpr: "eo::pfx_none",
        prefixedName: option.section,
        groupIdName: "ID_INVALID",
        aliasIdName: "ID_INVALID",
        flagsExpr: "0",
        visibilityExpr: "DefaultVis",
        param: 0,
        helpText: option.section,
        metaVar: "",
      });
    }
  }

  const specialEntries = [
    {
      idName: "ID_INPUT",
      kind: "Input",
      prefixExpr: "eo::pfx_none",
      prefixedName: "<input>",
      groupIdName: "ID_INVALID",
      aliasIdName: "ID_INVALID",
      flagsExpr: "0",
      visibilityExpr: "DefaultVis",
      param: 0,
      helpText: "input content",
      metaVar: "",
    },
    {
      idName: "ID_UNKNOWN",
      kind: "Unknown",
      prefixExpr: "eo::pfx_none",
      prefixedName: "<unknown>",
      groupIdName: "ID_INVALID",
      aliasIdName: "ID_INVALID",
      flagsExpr: "0",
      visibilityExpr: "DefaultVis",
      param: 0,
      helpText: "Unknown option",
      metaVar: "",
    },
  ];

  const optionEntries = [];

  for (const option of parsedOptions) {
    const groupIdName = groupIds.get(option.section);
    const canonicalId = idAllocator(`ID_${sanitizeIdentifier(option.longName)}`);
    const canonicalPrefixedName = `--${option.longName}`;
    const metaVar = normalizeMetaVar(option.valueSyntax);
    const hasValue = metaVar.length > 0;

    optionEntries.push({
      idName: canonicalId,
      kind: hasValue ? "Separate" : "Flag",
      prefixExpr: "eo::pfx_double",
      prefixedName: canonicalPrefixedName,
      groupIdName,
      aliasIdName: "ID_INVALID",
      flagsExpr: "0",
      visibilityExpr: "DefaultVis",
      param: 0,
      helpText: option.help,
      metaVar,
    });

    if (hasValue) {
      optionEntries.push({
        idName: idAllocator(`ID_${sanitizeIdentifier(option.longName)}_EQ`),
        kind: "Joined",
        prefixExpr: "eo::pfx_double",
        prefixedName: `${canonicalPrefixedName}=`,
        groupIdName,
        aliasIdName: canonicalId,
        flagsExpr: "0",
        visibilityExpr: "DefaultVis",
        param: 0,
        helpText: option.help,
        metaVar,
      });
    }

    const aliasSanitized = sanitizeIdentifier(option.alias.replace(/^-+/, ""));
    const aliasIdBase = idAllocator(`ID_alias_${aliasSanitized}`);
    optionEntries.push({
      idName: aliasIdBase,
      kind: hasValue ? "JoinedOrSeparate" : "Flag",
      prefixExpr: "eo::pfx_dash",
      prefixedName: option.alias,
      groupIdName,
      aliasIdName: canonicalId,
      flagsExpr: "0",
      visibilityExpr: "DefaultVis",
      param: 0,
      helpText: option.help,
      metaVar,
    });

    if (hasValue) {
      optionEntries.push({
        idName: idAllocator(`ID_alias_${aliasSanitized}_EQ`),
        kind: "Joined",
        prefixExpr: "eo::pfx_dash",
        prefixedName: `${option.alias}=`,
        groupIdName,
        aliasIdName: canonicalId,
        flagsExpr: "0",
        visibilityExpr: "DefaultVis",
        param: 0,
        helpText: option.help,
        metaVar,
      });
    }
  }

  optionEntries.sort(compareOptionEntriesForTableGen);

  const entries = [...groupEntries, ...specialEntries, ...optionEntries];
  return {
    entries,
    ids: entries.map((entry) => entry.idName),
    flags: collectBitMembers(entries.map((entry) => entry.flagsExpr), FLAG_BITS),
    visibility: collectBitMembers(entries.map((entry) => entry.visibilityExpr), VISIBILITY_BITS),
  };
}

function createIdAllocator() {
  const used = new Set(["ID_INVALID"]);
  return (base) => {
    let candidate = base;
    let index = 0;
    while (used.has(candidate)) {
      index += 1;
      candidate = `${base}_${index}`;
    }
    used.add(candidate);
    return candidate;
  };
}

function sanitizeIdentifier(value) {
  const sanitized = value
    .replace(/[^A-Za-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .replace(/_{2,}/g, "_");

  if (sanitized.length === 0) {
    return "anonymous";
  }
  if (/^[0-9]/.test(sanitized)) {
    return `_${sanitized}`;
  }
  return sanitized;
}

function firstPrefix(prefixExpr) {
  switch (prefixExpr) {
    case "eo::pfx_double":
      return "--";
    case "eo::pfx_dash":
      return "-";
    case "eo::pfx_none":
      return "";
    default:
      throw new Error(`unknown prefix expr ${prefixExpr}`);
  }
}

function optionNameForSort(entry) {
  const prefix = firstPrefix(entry.prefixExpr);
  return entry.prefixedName.startsWith(prefix)
    ? entry.prefixedName.slice(prefix.length)
    : entry.prefixedName;
}

function compareInsensitive(a, b) {
  const min = Math.min(a.length, b.length);
  for (let index = 0; index < min; index += 1) {
    const ca = a[index].toLowerCase();
    const cb = b[index].toLowerCase();
    if (ca === cb) {
      continue;
    }
    return ca < cb ? -1 : 1;
  }
  if (a.length === b.length) {
    return 0;
  }
  return a.length < b.length ? -1 : 1;
}

function compareOptName(a, b) {
  const min = Math.min(a.length, b.length);
  const prefixCompare = compareInsensitive(a.slice(0, min), b.slice(0, min));
  if (prefixCompare !== 0) {
    return prefixCompare;
  }
  if (a.length === b.length) {
    return 0;
  }
  return a.length === min ? 1 : -1;
}

function compareOptionEntriesForTableGen(lhs, rhs) {
  const lhsName = optionNameForSort(lhs);
  const rhsName = optionNameForSort(rhs);
  const nameCompare = compareOptName(lhsName, rhsName);
  if (nameCompare !== 0) {
    return nameCompare;
  }
  return lhs.prefixedName.localeCompare(rhs.prefixedName);
}

function normalizeMetaVar(valueSyntax) {
  return valueSyntax.trim();
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

function renderTsModule(model, nvccPath) {
  return `// Generated by scripts/js-gen/generate_nvcc_option.mjs.
// Source: ${nvccPath} --help

${renderEnum("NvccID", [["ID_INVALID", 0], ...model.ids.map((idName) => [idName, null])])}

${renderEnum("NvccFlag", model.flags)}

${renderEnum("NvccVisibility", model.visibility)}
`;
}

function renderHeader(model, nvccPath) {
  return `#pragma once

#include <eventide/option/opt_table.h>

namespace catter::opt::nvcc {

// Generated by scripts/js-gen/generate_nvcc_option.mjs.
// Source: ${nvccPath} --help
enum ID : unsigned {
    ID_INVALID = 0,
${model.ids.map((idName) => `    ${idName},`).join("\n")}
};

const eventide::option::OptTable& table();

}  // namespace catter::opt::nvcc
`;
}

function renderSource(model, nvccPath) {
  return `#include "opt/external/nvcc.h"

#include <array>
#include <span>

#include "opt/external/tablegen.h"

namespace catter::opt::nvcc {

namespace eo = eventide::option;

namespace detail {

using namespace catter::opt::external_detail;

// Generated by scripts/js-gen/generate_nvcc_option.mjs.
// Source: ${nvccPath} --help
constexpr auto OptionInfos = std::array<eo::OptTable::Info, ${model.entries.length}>{
${model.entries.map((entry) => renderInfoEntry(entry)).join(",\n")}
};

}  // namespace detail

const eo::OptTable& table() {
    static const auto opt_table = eo::OptTable(std::span<const eo::OptTable::Info>(detail::OptionInfos))
                                      .set_tablegen_mode(true)
                                      .set_dash_dash_parsing(true);
    return opt_table;
}

}  // namespace catter::opt::nvcc
`;
}

function renderInfoEntry(entry) {
  return `    eo::OptTable::Info{
        ._prefixes = ${entry.prefixExpr},
        ._prefixed_name = ${renderCppString(entry.prefixedName)},
        .id = ${entry.idName},
        .kind = ${entry.kind},
        .group_id = ${entry.groupIdName},
        .alias_id = ${entry.aliasIdName},
        .alias_args = nullptr,
        .flags = ${entry.flagsExpr},
        .visibility = ${entry.visibilityExpr},
        .param = ${entry.param},
        .help_text = ${renderCppString(entry.helpText)},
        .help_texts_for_variants = DefaultHelpVariants,
        .meta_var = ${renderCppNullableString(entry.metaVar)},
    }`;
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

function renderCppNullableString(value) {
  if (!value) {
    return "nullptr";
  }
  return renderCppString(value);
}

function renderCppString(value) {
  return JSON.stringify(value)
    .replace(/\u2028/g, "\\u2028")
    .replace(/\u2029/g, "\\u2029");
}
