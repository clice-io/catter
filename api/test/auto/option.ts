import { assertThrow } from "catter/debug";
import {
  ClangID,
  ClangVisibility,
  NvccID,
  collect,
  info,
  parse,
  replace,
  stringify,
  type OptionInfo,
  type OptionItem,
  type OptionTable,
} from "catter/option";

const OptionKindClass: {
  Group: number;
  Input: number;
  Unknown: number;
  Flag: number;
  Joined: number;
  Values: number;
  Separate: number;
  RemainingArgs: number;
  RemainingArgsJoined: number;
  CommaJoined: number;
  MultiArg: number;
  JoinedOrSeparate: number;
  JoinedAndSeparate: number;
} = {
  Group: 0,
  Input: 1,
  Unknown: 2,
  Flag: 3,
  Joined: 4,
  Values: 5,
  Separate: 6,
  CommaJoined: 7,
  MultiArg: 8,
  JoinedOrSeparate: 9,
  JoinedAndSeparate: 10,
  RemainingArgs: 11,
  RemainingArgsJoined: 12,
};

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function cloneItem(item: OptionItem): OptionItem {
  return {
    ...item,
    values: [...item.values],
  };
}

function infoById(table: OptionTable, id: number): OptionInfo {
  return info(table, {
    id,
    key: "",
    values: [],
    index: -1,
  });
}

function parseItemsFor(
  table: OptionTable,
  args: string[],
  label: string,
  visibility?: number,
): OptionItem[] {
  const parsed: OptionItem[] = [];
  parse(
    table,
    args,
    (parseRes) => {
      if (parseRes.isErr()) {
        throw new Error(`${label}: unexpected parse error: ${parseRes.error}`);
      }
      parsed.push(parseRes.value);
      return true;
    },
    visibility,
  );
  return parsed;
}

function parseItems(
  args: string[],
  label: string,
  visibility?: number,
): OptionItem[] {
  return parseItemsFor("clang", args, label, visibility);
}

function parseErrorsFor(
  table: OptionTable,
  args: string[],
  label: string,
): string[] {
  const errors: string[] = [];
  parse(table, args, (parseRes) => {
    if (parseRes.isErr()) {
      errors.push(parseRes.error);
      return true;
    }
    throw new Error(`${label}: expected parse error`);
  });
  return errors;
}

function parseErrors(args: string[], label: string): string[] {
  return parseErrorsFor("clang", args, label);
}

const parsed = parseItems(
  ["--all-warnings", "-Iinclude", "-o", "main.o", "--", "-dash.cc"],
  "basic parse",
);
expectEq(parsed.length, 4, "basic parse length");
assertThrow(
  parsed[0].key === "--all-warnings" &&
    parsed[0].values.length === 0 &&
    parsed[0].id === ClangID.ID_Wall,
);
assertThrow(
  parsed[1].key === "-I" &&
    parsed[1].values.length === 1 &&
    parsed[1].values[0] === "include",
);
assertThrow(
  parsed[2].key === "-o" &&
    parsed[2].values.length === 1 &&
    parsed[2].values[0] === "main.o",
);
assertThrow(parsed[3].key === "-dash.cc" && parsed[3].values.length === 0);

const seen = parseItems(["-fsyntax-only"], "single option").map(
  (item) => item.key,
);
expectEq(seen.length, 1, "single option length");
expectEq(seen[0], "-fsyntax-only", "single option key");

const errors = parseErrors(["-o"], "missing value");
assertThrow(errors.length === 1 && errors[0].includes("missing"));

const collected = collect("clang", ["--all-warnings", "-Iinclude", "main.cc"]);
assertThrow(collected.isOk());
if (!collected.isOk()) {
  throw new Error("collect should return parsed items for valid args");
}
expectEq(collected.value.length, 3, "collect parsed length");
expectEq(collected.value[0].key, "--all-warnings", "collect alias key");
expectEq(collected.value[0].id, ClangID.ID_Wall, "collect alias id");
expectEq(
  stringify("clang", collected.value[0]),
  "-Wall",
  "collect alias stringify",
);
expectEq(collected.value[1].key, "-I", "collect include key");
expectEq(collected.value[1].values[0], "include", "collect include value");
expectEq(collected.value[2].key, "main.cc", "collect input key");

const clangClDefaultVisible = parseItems(
  ["/c", "main.cc"],
  "clang cl visibility default",
);
expectEq(clangClDefaultVisible.length, 2, "clang cl default visibility length");
expectEq(clangClDefaultVisible[0].key, "/c", "clang cl default visibility key");

const clangClDriverOnlyVisible = parseItems(
  ["/c", "main.cc"],
  "clang cl visibility filtered",
  ClangVisibility.DefaultVis,
);
expectEq(
  clangClDriverOnlyVisible.length,
  2,
  "clang cl filtered visibility length",
);
expectEq(
  clangClDriverOnlyVisible[0].key,
  "/c",
  "clang cl filtered visibility key",
);
expectEq(
  clangClDriverOnlyVisible[1].key,
  "main.cc",
  "clang cl filtered visibility input",
);

const clangClFilteredUnknown = parseItems(
  ["/Foobj/main.obj", "/c", "main.cc"],
  "clang cl visibility filtered unknown",
  ClangVisibility.DefaultVis,
);
expectEq(clangClFilteredUnknown.length, 3, "clang cl filtered unknown length");
expectEq(
  clangClFilteredUnknown[0].key,
  "/Foobj/main.obj",
  "clang cl filtered unknown key",
);
expectEq(
  clangClFilteredUnknown[1].key,
  "/c",
  "clang cl filtered unknown option",
);
expectEq(
  clangClFilteredUnknown[2].key,
  "main.cc",
  "clang cl filtered unknown input",
);

const clangClFilteredMissingValue = parseItems(
  ["/Fo"],
  "clang cl visibility filtered missing value",
  ClangVisibility.DefaultVis,
);
expectEq(
  clangClFilteredMissingValue.length,
  1,
  "clang cl filtered missing value length",
);
expectEq(
  clangClFilteredMissingValue[0].key,
  "/Fo",
  "clang cl filtered missing value key",
);

const clangClOutputVisible = parseItems(
  ["/Foobj/main.obj", "/Fe:bin/tool.exe"],
  "clang cl output visibility filtered",
  ClangVisibility.DefaultVis | ClangVisibility.CLOption,
);
expectEq(clangClOutputVisible.length, 2, "clang cl output visibility length");
expectEq(clangClOutputVisible[0].key, "/Fo", "clang cl output object key");
expectEq(
  clangClOutputVisible[0].values[0],
  "obj/main.obj",
  "clang cl output object value",
);
expectEq(clangClOutputVisible[1].key, "/Fe:", "clang cl output executable key");
expectEq(
  clangClOutputVisible[1].id,
  ClangID.ID__SLASH_Fe,
  "clang cl output executable id",
);

const clangClAllVisible = parseItems(
  ["/c", "main.cc"],
  "clang cl visibility all",
  0xffff_ffff,
);
expectEq(clangClAllVisible.length, 2, "clang cl all visibility length");
expectEq(clangClAllVisible[0].key, "/c", "clang cl all visibility key");

const clangHiddenSeparateUnknown = parseItems(
  ["-target", "x86_64-pc-windows-msvc", "main.cc"],
  "clang hidden separate unknown",
  ClangVisibility.CC1Option,
);
expectEq(
  clangHiddenSeparateUnknown.length,
  3,
  "clang hidden separate unknown length",
);
expectEq(
  clangHiddenSeparateUnknown[0].key,
  "-target",
  "clang hidden separate unknown key",
);
expectEq(
  clangHiddenSeparateUnknown[1].key,
  "x86_64-pc-windows-msvc",
  "clang hidden separate unknown value",
);
expectEq(
  clangHiddenSeparateUnknown[2].key,
  "main.cc",
  "clang hidden separate unknown input",
);

const collectError = collect("clang", ["-o"]);
assertThrow(collectError.isErr());
if (!collectError.isErr()) {
  throw new Error("collect should return an error string for invalid args");
}
assertThrow(collectError.error.includes("missing"));

const nvccParsed = parseItemsFor(
  "nvcc",
  ["-ofoo.o", "-I=include", "--std=c++17", "-no-align-double", "kernel.cu"],
  "nvcc basic parse",
);
expectEq(nvccParsed.length, 5, "nvcc parsed length");
expectEq(nvccParsed[0].id, NvccID.ID_output_file, "nvcc output id");
expectEq(
  stringify("nvcc", nvccParsed[0]),
  "--output-file foo.o",
  "nvcc stringify output",
);
expectEq(
  stringify("nvcc", nvccParsed[1]),
  "--include-path include",
  "nvcc stringify include",
);
expectEq(stringify("nvcc", nvccParsed[2]), "--std c++17", "nvcc stringify std");
expectEq(
  stringify("nvcc", nvccParsed[3]),
  "--no-align-double",
  "nvcc stringify flag alias",
);
expectEq(nvccParsed[4].key, "kernel.cu", "nvcc input key");

const nvccOutputInfo = infoById("nvcc", NvccID.ID_output_file);
expectEq(
  nvccOutputInfo.kind,
  OptionKindClass.Separate,
  "nvcc output info kind",
);
expectEq(nvccOutputInfo.prefixedKey, "--output-file", "nvcc output info key");
expectEq(nvccOutputInfo.meta_var, "<file>", "nvcc output meta var");

const nvccHelpInfo = infoById("nvcc", NvccID.ID_help);
expectEq(nvccHelpInfo.kind, OptionKindClass.Flag, "nvcc help info kind");
expectEq(nvccHelpInfo.prefixedKey, "--help", "nvcc help key");

const nvccErrors = parseErrorsFor("nvcc", ["-o"], "nvcc missing value");
assertThrow(nvccErrors.length === 1 && nvccErrors[0].includes("missing"));

const nvccUnknown = parseItemsFor(
  "nvcc",
  ["--definitely-not-a-real-nvcc-flag"],
  "nvcc unknown",
);
expectEq(nvccUnknown.length, 1, "nvcc unknown parsed length");
expectEq(nvccUnknown[0].id, NvccID.ID_UNKNOWN, "nvcc unknown id");
expectEq(
  stringify("nvcc", nvccUnknown[0]),
  "--definitely-not-a-real-nvcc-flag",
  "nvcc unknown stringify",
);

const includeInfo = info("clang", parsed[1]);
assertThrow(
  includeInfo.id === parsed[1].id &&
    includeInfo.prefixedKey === "-I" &&
    Array.isArray(includeInfo.aliasArgs),
);
expectEq(
  includeInfo.kind,
  OptionKindClass.JoinedOrSeparate,
  "include info kind",
);

const inputInfo = info("clang", parsed[3]);
expectEq(inputInfo.prefixedKey, "<input>", "input prefixed key");
expectEq(inputInfo.kind, OptionKindClass.Input, "input info kind");

assertThrow(
  parsed[0].id === ClangID.ID_Wall &&
    parsed[0].key === "--all-warnings" &&
    parsed[0].values.length === 0,
);

const aliasString = stringify("clang", parsed[0]);
const includeString = stringify("clang", parsed[1]);
const outputString = stringify("clang", parsed[2]);
const inputString = stringify("clang", parsed[3]);
expectEq(aliasString, "-Wall", "stringify alias");
expectEq(includeString, "-I include", "stringify include");
expectEq(outputString, "-o main.o", "stringify output");
expectEq(inputString, "-dash.cc", "stringify input");

let invalidFailed = false;
try {
  infoById("clang", 0);
} catch {
  invalidFailed = true;
}
assertThrow(invalidFailed);

const unknownParsed = parseItems(
  ["--definitely-not-a-real-clang-flag"],
  "unknown",
);
expectEq(unknownParsed.length, 1, "unknown parsed length");
expectEq(unknownParsed[0].id, ClangID.ID_UNKNOWN, "unknown option id");
expectEq(
  stringify("clang", unknownParsed[0]),
  "--definitely-not-a-real-clang-flag",
  "stringify unknown",
);
const unknownInfo = info("clang", unknownParsed[0]);
expectEq(unknownInfo.kind, OptionKindClass.Unknown, "unknown info kind");

const livenessParsed = parseItems(
  ["-fextend-variable-liveness"],
  "alias with alias args",
);
expectEq(livenessParsed.length, 1, "liveness parsed length");
expectEq(
  stringify("clang", livenessParsed[0]),
  "-fextend-variable-liveness=all",
  "stringify liveness alias",
);
expectEq(livenessParsed[0].values.length, 1, "liveness parsed values length");
expectEq(livenessParsed[0].values[0], "all", "liveness parsed value");

const optimizeParsed = parseItems(
  ["--optimize"],
  "alias to joined without alias args",
);
expectEq(optimizeParsed.length, 1, "optimize parsed length");
expectEq(stringify("clang", optimizeParsed[0]), "-O", "stringify optimize");
expectEq(optimizeParsed[0].id, ClangID.ID_O, "optimize parsed id");
expectEq(optimizeParsed[0].values.length, 1, "optimize parsed values length");
expectEq(optimizeParsed[0].values[0], "", "optimize parsed empty value");

const sanitizeParsed = parseItems(
  ["-fsanitize=address,undefined"],
  "comma joined parse",
);
expectEq(sanitizeParsed.length, 1, "sanitize parsed length");
const sanitizeInfo = info("clang", sanitizeParsed[0]);
expectEq(sanitizeInfo.kind, OptionKindClass.CommaJoined, "sanitize info kind");
expectEq(sanitizeParsed[0].values.length, 2, "sanitize values length");
expectEq(sanitizeParsed[0].values[0], "address", "sanitize first value");
expectEq(sanitizeParsed[0].values[1], "undefined", "sanitize second value");
expectEq(
  stringify("clang", sanitizeParsed[0]),
  "-fsanitize=address,undefined",
  "stringify sanitize",
);

const xopenmpParsed = parseItems(
  ["-Xopenmp-target=x86_64-unknown-linux-gnu", "-fsyntax-only"],
  "joined and separate parse",
);
expectEq(xopenmpParsed.length, 1, "xopenmp parsed length");
const xopenmpInfo = info("clang", xopenmpParsed[0]);
expectEq(
  xopenmpInfo.kind,
  OptionKindClass.JoinedAndSeparate,
  "xopenmp info kind",
);
expectEq(xopenmpParsed[0].values.length, 2, "xopenmp values length");
expectEq(
  xopenmpParsed[0].values[0],
  "x86_64-unknown-linux-gnu",
  "xopenmp joined value",
);
expectEq(xopenmpParsed[0].values[1], "-fsyntax-only", "xopenmp separate value");
expectEq(
  stringify("clang", xopenmpParsed[0]),
  "-Xopenmp-target=x86_64-unknown-linux-gnu -fsyntax-only",
  "stringify xopenmp",
);

const linkParsed = parseItems(["-ldl"], "render joined parse");
expectEq(linkParsed.length, 1, "link parsed length");
const linkInfo = info("clang", linkParsed[0]);
expectEq(linkInfo.kind, OptionKindClass.JoinedOrSeparate, "link info kind");
expectEq(linkParsed[0].values.length, 1, "link values length");
expectEq(linkParsed[0].values[0], "dl", "link value");
expectEq(stringify("clang", linkParsed[0]), "-ldl", "stringify link");

const segaddrParsed = parseItems(
  ["-segaddr", "__TEXT", "0x1000"],
  "multi arg parse",
);
expectEq(segaddrParsed.length, 1, "segaddr parsed length");
const segaddrInfo = info("clang", segaddrParsed[0]);
expectEq(segaddrInfo.kind, OptionKindClass.MultiArg, "segaddr info kind");
expectEq(segaddrParsed[0].values.length, 2, "segaddr values length");
expectEq(segaddrParsed[0].values[0], "__TEXT", "segaddr first value");
expectEq(segaddrParsed[0].values[1], "0x1000", "segaddr second value");
expectEq(
  stringify("clang", segaddrParsed[0]),
  "-segaddr __TEXT 0x1000",
  "stringify segaddr",
);

const multiArgErrors = parseErrors(
  ["-segaddr", "__TEXT"],
  "multi arg missing values",
);
assertThrow(
  multiArgErrors.length === 1 && multiArgErrors[0].includes("missing"),
);

const stoppedKeys: string[] = [];
parse("clang", ["-fsyntax-only", "-Winvalid-stop-check"], (parseRes) => {
  if (parseRes.isErr()) {
    throw new Error(`stop parse: unexpected parse error: ${parseRes.error}`);
  }
  stoppedKeys.push(parseRes.value.key);
  return false;
});
expectEq(stoppedKeys.length, 1, "stop parse length");
expectEq(stoppedKeys[0], "-fsyntax-only", "stop parse first key");

// a xmake demo
const command =
  "-o build/linux/x86_64/debug/catter build/.objs/catter/linux/x86_64/debug/src/catter/main.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/ipc.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/session.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/os.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/option.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/io.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/service.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/fs.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/apitool.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/js.cc.o build/.objs/catter-core/linux/x86_64/debug/api/output/lib/lib.js.o -m64 -L/home/kacent/.xmake/packages/q/quickjs-ng/v0.11.0/3b0b0541a046418183a839d92c0ee676/lib -Lbuild/linux/x86_64/debug -L/home/kacent/.xmake/packages/s/spdlog/v1.15.3/30816fde81524216904b4fedec0afba9/lib -L/home/kacent/.xmake/packages/k/kotatsu/66/633f0ffaa3c04216a51733d87ad6e471/lib -L/home/kacent/.xmake/packages/l/libuv/v1.52.0/36f98318201548a8ba03dcfff7683ae4/lib -L/home/kacent/.xmake/packages/c/cpptrace/v1.0.4/9e29ee9be85b4fd08157d98d4b9e2c49/lib -L/home/kacent/.xmake/packages/l/libdwarf/2.3.0/a4d27336f566462e80a33b85f1aec162/lib -L/home/kacent/.xmake/packages/z/zlib/v1.3.1/db68dfed70ca4c0b92a3b0b946951d79/lib -L/home/kacent/.xmake/packages/z/zstd/v1.5.7/bbc2fa368000410da812e97c13ccbbe3/lib -lqjs -lcommon -lspdlogd -lztest -loption -lasync -luv -lcpptrace -ldwarf -lz -lzstd -lm -lpthread -ldl -fsanitize=address".split(
    " ",
  );
const demoParsed: OptionItem[] = [];
parse("clang", command, (parseRes) => {
  assertThrow(parseRes.isOk());
  if (parseRes.isOk()) {
    demoParsed.push(parseRes.value);
  }
  return true;
});
assertThrow(demoParsed.length > 10);
expectEq(
  stringify("clang", demoParsed[0]),
  "-o build/linux/x86_64/debug/catter",
  "stringify demo output",
);
const spdlogLink = demoParsed.find(
  (item) => stringify("clang", item) === "-lspdlogd",
);
assertThrow(spdlogLink !== undefined);

// xmake clang-cl/msvc demos
const clStyleVisibility = ClangVisibility.DefaultVis | ClangVisibility.CLOption;

const xmakeClangClCompileCommand = [
  "-c",
  "--target=x86_64-pc-windows-msvc",
  "-MD",
  "-Zi",
  "-FS",
  "-Fdbuild-clang-cl\\windows\\x64\\debug\\compile.catter-proxy.pdb",
  "-Od",
  "-std:c++latest",
  "-ID:\\Code\\catter\\.pixi\\envs\\dev\\include",
  "-ID:\\Code\\catter\\.pixi\\envs\\dev\\Library\\include",
  "-Isrc\\catter-proxy",
  "-Isrc\\common",
  "-Isrc\\catter-hook",
  "-DDEBUG",
  "-DCATTER_WINDOWS",
  "-DWIN32_LEAN_AND_MEAN",
  "-DNOMINMAX",
  "-DSPDLOG_COMPILED_LIB",
  "-DSPDLOG_USE_STD_FORMAT",
  "-DSPDLOG_NO_EXCEPTIONS",
  "-DCPPTRACE_STATIC_DEFINE",
  "-DCURL_STATICLIB",
  "/EHsc",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\s\\spdlog\\v1.15.3\\4a99064252c44ca78d37bd17a2e7c30c\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\k\\kotatsu\\136\\b068933dccdd4ea987509f51e463d430\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\l\\libuv\\v1.52.0\\e304f43af2504d1c918d995857fa35c6\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\c\\cpptrace\\v1.0.4\\d44e7cb00b5b4402995eeb8f77051ea4\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\l\\libcurl\\8.11.0\\f37b877819304ceda1a0e39cc7de546b\\include",
  "-Fobuild-clang-cl\\.objs\\catter-proxy\\windows\\x64\\debug\\src\\catter-proxy\\main.cc.obj",
  "src\\catter-proxy\\main.cc",
];
const xmakeClangClParsed = parseItems(
  xmakeClangClCompileCommand,
  "xmake clang-cl compile",
  clStyleVisibility,
);
assertThrow(xmakeClangClParsed.length > 20);
const xmakeClangClObject = xmakeClangClParsed.find(
  (item) => item.key === "-Fo",
);
assertThrow(xmakeClangClObject !== undefined);
if (xmakeClangClObject === undefined) {
  throw new Error("xmake clang-cl compile: expected -Fo item");
}
expectEq(
  xmakeClangClObject.values[0],
  "build-clang-cl\\.objs\\catter-proxy\\windows\\x64\\debug\\src\\catter-proxy\\main.cc.obj",
  "xmake clang-cl object value",
);
assertThrow(
  xmakeClangClParsed.some((item) => item.key === "src\\catter-proxy\\main.cc"),
);

const xmakeMsvcCompileCommand = [
  "-c",
  "-nologo",
  "-MD",
  "-Zi",
  "-FS",
  "-Fdbuild-msvc\\windows\\x64\\debug\\compile.catter-proxy.pdb",
  "-Od",
  "-std:c++23preview",
  "-ID:\\Code\\catter\\.pixi\\envs\\dev\\include",
  "-ID:\\Code\\catter\\.pixi\\envs\\dev\\Library\\include",
  "-Isrc\\catter-proxy",
  "-Isrc\\common",
  "-Isrc\\catter-hook",
  "-DDEBUG",
  "-DCATTER_WINDOWS",
  "-DWIN32_LEAN_AND_MEAN",
  "-DNOMINMAX",
  "-DSPDLOG_COMPILED_LIB",
  "-DSPDLOG_USE_STD_FORMAT",
  "-DSPDLOG_NO_EXCEPTIONS",
  "-DCPPTRACE_STATIC_DEFINE",
  "-DCURL_STATICLIB",
  "/EHsc",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\s\\spdlog\\v1.15.3\\959c9024cc404a348d5232ffde4c6d84\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\k\\kotatsu\\136\\262f47376c5845d9ad0444a7bff7d846\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\l\\libuv\\v1.52.0\\2cf65b84931d47d9a85f7d4937f087e5\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\c\\cpptrace\\v1.0.4\\651432b72b1c4ca193e4519e92bdc74f\\include",
  "-external:W0",
  "-external:ID:\\pkg\\xmake\\pkg-cache\\l\\libcurl\\8.11.0\\a95976b98e2f4abc95e359beeead985b\\include",
  "-fsanitize=address",
  "-Fobuild-msvc\\.objs\\catter-proxy\\windows\\x64\\debug\\src\\catter-proxy\\main.cc.obj",
  "src\\catter-proxy\\main.cc",
];
const xmakeMsvcParsed = parseItems(
  xmakeMsvcCompileCommand,
  "xmake msvc compile",
  clStyleVisibility,
);
assertThrow(xmakeMsvcParsed.length > 20);
const xmakeMsvcObject = xmakeMsvcParsed.find((item) => item.key === "-Fo");
assertThrow(xmakeMsvcObject !== undefined);
if (xmakeMsvcObject === undefined) {
  throw new Error("xmake msvc compile: expected -Fo item");
}
expectEq(
  xmakeMsvcObject.values[0],
  "build-msvc\\.objs\\catter-proxy\\windows\\x64\\debug\\src\\catter-proxy\\main.cc.obj",
  "xmake msvc object value",
);
assertThrow(
  xmakeMsvcParsed.some((item) => item.key === "src\\catter-proxy\\main.cc"),
);

// replace

const newCmd = replace(
  "clang",
  "-o build/linux/x86_64/debug/catter build/.objs/catter/linux/x86_64/debug/src/catter/main.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/ipc.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/session.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/os.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/option.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/io.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/service.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/fs.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/apitool.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/js.cc.o build/.objs/catter-core/linux/x86_64/debug/api/output/lib/lib.js.o -m64 -L/home/kacent/.xmake/packages/q/quickjs-ng/v0.11.0/f2160e7e0cf64779a62dd4ae96ef37e2/lib -Lbuild/linux/x86_64/debug -L/home/kacent/.xmake/packages/s/spdlog/v1.15.3/1a3282179c394ba089a988a4f35f26b0/lib -L/home/kacent/.xmake/packages/k/kotatsu/73/2189f84b06ee4d8fb12f20abf385213d/lib -L/home/kacent/.xmake/packages/l/libuv/v1.52.0/e0b29ff5583447e5833cc60c24aaa354/lib -L/home/kacent/.xmake/packages/c/cpptrace/v1.0.4/efd2fd5f2c774566a77fe4c35ab9ee2c/lib -L/home/kacent/.xmake/packages/l/libdwarf/2.3.0/1643c83c9f2b454ea5b223360a7abb8b/lib -L/home/kacent/.xmake/packages/z/zlib/v1.3.1/47f5907ebdb04479a4e2679f95ffa2b5/lib -L/home/kacent/.xmake/packages/z/zstd/v1.5.7/8b39fe4c8c5149d0a3747878b6e2dafc/lib -lqjs -lcommon -lspdlogd -lztest -loption -lasync -luv -lcpptrace -ldwarf -lz -lzstd -lm -lpthread -ldl -fsanitize=address".split(
    " ",
  ),
  (parseRes) => {
    if (!parseRes.isOk()) {
      throw new Error("replace: unexpected parse error");
    }
    switch (parseRes.value.id as ClangID) {
      case ClangID.ID_o:
        return "-o 233";
      case ClangID.ID_INPUT:
        return "<input>";
      case ClangID.ID_L:
        return "-L<...>";
      case ClangID.ID_fsanitize_EQ:
        return "san!";
      default:
        return true;
    }
  },
);
expectEq(
  newCmd,
  "-o 233 <input> <input> <input> <input> <input> <input> <input> <input> <input> <input> <input> -m64 -L<...> -L<...> -L<...> -L<...> -L<...> -L<...> -L<...> -L<...> -L<...> -lqjs -lcommon -lspdlogd -lztest -loption -lasync -luv -lcpptrace -ldwarf -lz -lzstd -lm -lpthread -ldl san!",
  "replace",
);
