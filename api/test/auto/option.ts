import { cmd, debug, io, option } from "catter";

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

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function cloneItem(item: option.OptionItem): option.OptionItem {
  return {
    ...item,
    values: [...item.values],
  };
}

function infoById(table: option.OptionTable, id: number): option.OptionInfo {
  return option.info(table, {
    id,
    key: "",
    values: [],
    index: -1,
  });
}

function parseItemsFor(
  table: option.OptionTable,
  args: string[],
  label: string,
  visibility?: number,
): option.OptionItem[] {
  const parsed: option.OptionItem[] = [];
  option.parse(
    table,
    args,
    (parseRes) => {
      if (typeof parseRes === "string") {
        throw new Error(`${label}: unexpected parse error: ${parseRes}`);
      }
      parsed.push(parseRes);
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
): option.OptionItem[] {
  return parseItemsFor("clang", args, label, visibility);
}

function parseErrorsFor(
  table: option.OptionTable,
  args: string[],
  label: string,
): string[] {
  const errors: string[] = [];
  option.parse(table, args, (parseRes) => {
    if (typeof parseRes === "string") {
      errors.push(parseRes);
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
debug.assertThrow(
  parsed[0].key === "--all-warnings" &&
    parsed[0].values.length === 0 &&
    typeof parsed[0].unalias === "number",
);
debug.assertThrow(
  parsed[1].key === "-I" &&
    parsed[1].values.length === 1 &&
    parsed[1].values[0] === "include",
);
debug.assertThrow(
  parsed[2].key === "-o" &&
    parsed[2].values.length === 1 &&
    parsed[2].values[0] === "main.o",
);
debug.assertThrow(
  parsed[3].key === "-dash.cc" && parsed[3].values.length === 0,
);

const seen = parseItems(["-fsyntax-only"], "single option").map(
  (item) => item.key,
);
expectEq(seen.length, 1, "single option length");
expectEq(seen[0], "-fsyntax-only", "single option key");

const errors = parseErrors(["-o"], "missing value");
debug.assertThrow(errors.length === 1 && errors[0].includes("missing"));

const collected = option.collect("clang", [
  "--all-warnings",
  "-Iinclude",
  "main.cc",
]);
debug.assertThrow(Array.isArray(collected));
if (!Array.isArray(collected)) {
  throw new Error("collect should return parsed items for valid args");
}
expectEq(collected.length, 3, "collect parsed length");
expectEq(collected[0].key, "--all-warnings", "collect alias key");
debug.assertThrow(typeof collected[0].unalias === "number");
expectEq(
  option.stringify("clang", collected[0]),
  "-Wall",
  "collect alias stringify",
);
expectEq(collected[1].key, "-I", "collect include key");
expectEq(collected[1].values[0], "include", "collect include value");
expectEq(collected[2].key, "main.cc", "collect input key");

const clangClDefaultVisible = parseItems(
  ["/c", "main.cc"],
  "clang cl visibility default",
);
expectEq(clangClDefaultVisible.length, 2, "clang cl default visibility length");
expectEq(clangClDefaultVisible[0].key, "/c", "clang cl default visibility key");

const clangClDriverOnlyVisible = parseItems(
  ["/c", "main.cc"],
  "clang cl visibility filtered",
  option.ClangVisibility.DefaultVis,
);
expectEq(
  clangClDriverOnlyVisible.length,
  1,
  "clang cl filtered visibility length",
);
expectEq(
  clangClDriverOnlyVisible[0].key,
  "main.cc",
  "clang cl filtered visibility input",
);

const clangClAllVisible = parseItems(
  ["/c", "main.cc"],
  "clang cl visibility all",
  0xffff_ffff,
);
expectEq(clangClAllVisible.length, 2, "clang cl all visibility length");
expectEq(clangClAllVisible[0].key, "/c", "clang cl all visibility key");

const collectError = option.collect("clang", ["-o"]);
debug.assertThrow(typeof collectError === "string");
if (typeof collectError !== "string") {
  throw new Error("collect should return an error string for invalid args");
}
debug.assertThrow(collectError.includes("missing"));

const nvccParsed = parseItemsFor(
  "nvcc",
  ["-ofoo.o", "-I=include", "--std=c++17", "-no-align-double", "kernel.cu"],
  "nvcc basic parse",
);
expectEq(nvccParsed.length, 5, "nvcc parsed length");
debug.assertThrow(typeof nvccParsed[0].unalias === "number");
expectEq(
  option.stringify("nvcc", nvccParsed[0]),
  "--output-file foo.o",
  "nvcc stringify output",
);
expectEq(
  option.stringify("nvcc", nvccParsed[1]),
  "--include-path include",
  "nvcc stringify include",
);
expectEq(
  option.stringify("nvcc", nvccParsed[2]),
  "--std c++17",
  "nvcc stringify std",
);
expectEq(
  option.stringify("nvcc", nvccParsed[3]),
  "--no-align-double",
  "nvcc stringify flag alias",
);
expectEq(nvccParsed[4].key, "kernel.cu", "nvcc input key");

const nvccOutputCopy = cloneItem(nvccParsed[0]);
option.convertToUnalias("nvcc", nvccOutputCopy);
expectEq(
  nvccOutputCopy.id,
  option.NvccID.ID_output_file,
  "nvcc output unalias id",
);
expectEq(nvccOutputCopy.key, "--output-file", "nvcc output unalias key");
expectEq(nvccOutputCopy.values[0], "foo.o", "nvcc output unalias value");

const nvccFlagCopy = cloneItem(nvccParsed[3]);
option.convertToUnalias("nvcc", nvccFlagCopy);
expectEq(
  nvccFlagCopy.id,
  option.NvccID.ID_no_align_double,
  "nvcc flag unalias id",
);
expectEq(nvccFlagCopy.key, "--no-align-double", "nvcc flag unalias key");
expectEq(nvccFlagCopy.values.length, 0, "nvcc flag unalias values length");

const nvccOutputInfo = infoById("nvcc", option.NvccID.ID_output_file);
expectEq(
  nvccOutputInfo.kind,
  OptionKindClass.SeparateClass,
  "nvcc output info kind",
);
expectEq(nvccOutputInfo.prefixedKey, "--output-file", "nvcc output info key");
expectEq(nvccOutputInfo.meta_var, "<file>", "nvcc output meta var");

const nvccHelpInfo = infoById("nvcc", option.NvccID.ID_help);
expectEq(nvccHelpInfo.kind, OptionKindClass.FlagClass, "nvcc help info kind");
expectEq(nvccHelpInfo.prefixedKey, "--help", "nvcc help key");

const nvccErrors = parseErrorsFor("nvcc", ["-o"], "nvcc missing value");
debug.assertThrow(nvccErrors.length === 1 && nvccErrors[0].includes("missing"));

const nvccUnknown = parseItemsFor(
  "nvcc",
  ["--definitely-not-a-real-nvcc-flag"],
  "nvcc unknown",
);
expectEq(nvccUnknown.length, 1, "nvcc unknown parsed length");
expectEq(nvccUnknown[0].id, option.NvccID.ID_UNKNOWN, "nvcc unknown id");
expectEq(
  option.stringify("nvcc", nvccUnknown[0]),
  "--definitely-not-a-real-nvcc-flag",
  "nvcc unknown stringify",
);

const includeInfo = option.info("clang", parsed[1]);
debug.assertThrow(
  includeInfo.id === parsed[1].id &&
    includeInfo.prefixedKey === "-I" &&
    Array.isArray(includeInfo.aliasArgs),
);
expectEq(
  includeInfo.kind,
  OptionKindClass.JoinedOrSeparateClass,
  "include info kind",
);

const inputInfo = option.info("clang", parsed[3]);
expectEq(inputInfo.prefixedKey, "<input>", "input prefixed key");
expectEq(inputInfo.kind, OptionKindClass.InputClass, "input info kind");

const aliasTargetId = parsed[0].unalias;
debug.assertThrow(typeof aliasTargetId === "number");
if (typeof aliasTargetId !== "number") {
  throw new Error("expected alias target for --all-warnings");
}

const aliasCopy = cloneItem(parsed[0]);
const convertedAlias = option.convertToUnalias("clang", aliasCopy);
debug.assertThrow(convertedAlias === aliasCopy);
debug.assertThrow(
  convertedAlias.id === aliasTargetId &&
    convertedAlias.unalias === undefined &&
    convertedAlias.key === "-Wall" &&
    convertedAlias.values.length === 0,
);
debug.assertThrow(parsed[0].id !== aliasTargetId);
debug.assertThrow(typeof parsed[0].unalias === "number");
debug.assertThrow(parsed[0].values.length === 0);

const includeCopy = cloneItem(parsed[1]);
const unchangedInclude = option.convertToUnalias("clang", includeCopy);
debug.assertThrow(unchangedInclude === includeCopy);
expectEq(unchangedInclude.id, parsed[1].id, "convertToUnalias no-op id");
expectEq(unchangedInclude.key, parsed[1].key, "convertToUnalias no-op key");
expectEq(
  unchangedInclude.values[0],
  parsed[1].values[0],
  "convertToUnalias no-op values",
);

const aliasString = option.stringify("clang", parsed[0]);
const includeString = option.stringify("clang", parsed[1]);
const outputString = option.stringify("clang", parsed[2]);
const inputString = option.stringify("clang", parsed[3]);
expectEq(aliasString, "-Wall", "stringify alias");
expectEq(includeString, "-I include", "stringify include");
expectEq(outputString, "-o main.o", "stringify output");
expectEq(inputString, "-dash.cc", "stringify input");
debug.assertThrow(typeof parsed[0].unalias === "number");
debug.assertThrow(parsed[0].values.length === 0);

let invalidFailed = false;
try {
  infoById("clang", 0);
} catch {
  invalidFailed = true;
}
debug.assertThrow(invalidFailed);

const unknownParsed = parseItems(
  ["--definitely-not-a-real-clang-flag"],
  "unknown",
);
expectEq(unknownParsed.length, 1, "unknown parsed length");
expectEq(unknownParsed[0].id, option.ClangID.ID_UNKNOWN, "unknown option id");
expectEq(
  option.stringify("clang", unknownParsed[0]),
  "--definitely-not-a-real-clang-flag",
  "stringify unknown",
);
const unknownInfo = option.info("clang", unknownParsed[0]);
expectEq(unknownInfo.kind, OptionKindClass.UnknownClass, "unknown info kind");

const livenessParsed = parseItems(
  ["-fextend-variable-liveness"],
  "alias with alias args",
);
expectEq(livenessParsed.length, 1, "liveness parsed length");
debug.assertThrow(typeof livenessParsed[0].unalias === "number");
const livenessInfo = option.info("clang", livenessParsed[0]);
debug.assertThrow(
  livenessInfo.aliasArgs.length === 1 && livenessInfo.aliasArgs[0] === "all",
);
const livenessCopy = cloneItem(livenessParsed[0]);
option.convertToUnalias("clang", livenessCopy);
expectEq(
  livenessCopy.key,
  "-fextend-variable-liveness=",
  "liveness unaliased key",
);
expectEq(livenessCopy.values.length, 1, "liveness unaliased values length");
expectEq(livenessCopy.values[0], "all", "liveness unaliased value");
expectEq(
  option.stringify("clang", livenessParsed[0]),
  "-fextend-variable-liveness=all",
  "stringify liveness alias",
);
debug.assertThrow(livenessParsed[0].values.length === 0);
debug.assertThrow(typeof livenessParsed[0].unalias === "number");

const optimizeParsed = parseItems(
  ["--optimize"],
  "alias to joined without alias args",
);
expectEq(optimizeParsed.length, 1, "optimize parsed length");
debug.assertThrow(typeof optimizeParsed[0].unalias === "number");
const optimizeCopy = cloneItem(optimizeParsed[0]);
option.convertToUnalias("clang", optimizeCopy);
expectEq(optimizeCopy.key, "-O", "optimize unaliased key");
expectEq(optimizeCopy.values.length, 1, "optimize unaliased values length");
expectEq(optimizeCopy.values[0], "", "optimize unaliased empty value");
expectEq(
  option.stringify("clang", optimizeParsed[0]),
  "-O",
  "stringify optimize",
);
debug.assertThrow(optimizeParsed[0].values.length === 0);
debug.assertThrow(typeof optimizeParsed[0].unalias === "number");

const sanitizeParsed = parseItems(
  ["-fsanitize=address,undefined"],
  "comma joined parse",
);
expectEq(sanitizeParsed.length, 1, "sanitize parsed length");
const sanitizeInfo = option.info("clang", sanitizeParsed[0]);
expectEq(
  sanitizeInfo.kind,
  OptionKindClass.CommaJoinedClass,
  "sanitize info kind",
);
expectEq(sanitizeParsed[0].values.length, 2, "sanitize values length");
expectEq(sanitizeParsed[0].values[0], "address", "sanitize first value");
expectEq(sanitizeParsed[0].values[1], "undefined", "sanitize second value");
expectEq(
  option.stringify("clang", sanitizeParsed[0]),
  "-fsanitize=address,undefined",
  "stringify sanitize",
);

const xopenmpParsed = parseItems(
  ["-Xopenmp-target=x86_64-unknown-linux-gnu", "-fsyntax-only"],
  "joined and separate parse",
);
expectEq(xopenmpParsed.length, 1, "xopenmp parsed length");
const xopenmpInfo = option.info("clang", xopenmpParsed[0]);
expectEq(
  xopenmpInfo.kind,
  OptionKindClass.JoinedAndSeparateClass,
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
  option.stringify("clang", xopenmpParsed[0]),
  "-Xopenmp-target=x86_64-unknown-linux-gnu -fsyntax-only",
  "stringify xopenmp",
);

const linkParsed = parseItems(["-ldl"], "render joined parse");
expectEq(linkParsed.length, 1, "link parsed length");
const linkInfo = option.info("clang", linkParsed[0]);
expectEq(
  linkInfo.kind,
  OptionKindClass.JoinedOrSeparateClass,
  "link info kind",
);
expectEq(linkParsed[0].values.length, 1, "link values length");
expectEq(linkParsed[0].values[0], "dl", "link value");
expectEq(option.stringify("clang", linkParsed[0]), "-ldl", "stringify link");

const segaddrParsed = parseItems(
  ["-segaddr", "__TEXT", "0x1000"],
  "multi arg parse",
);
expectEq(segaddrParsed.length, 1, "segaddr parsed length");
const segaddrInfo = option.info("clang", segaddrParsed[0]);
expectEq(segaddrInfo.kind, OptionKindClass.MultiArgClass, "segaddr info kind");
expectEq(segaddrParsed[0].values.length, 2, "segaddr values length");
expectEq(segaddrParsed[0].values[0], "__TEXT", "segaddr first value");
expectEq(segaddrParsed[0].values[1], "0x1000", "segaddr second value");
expectEq(
  option.stringify("clang", segaddrParsed[0]),
  "-segaddr __TEXT 0x1000",
  "stringify segaddr",
);

const multiArgErrors = parseErrors(
  ["-segaddr", "__TEXT"],
  "multi arg missing values",
);
debug.assertThrow(
  multiArgErrors.length === 1 && multiArgErrors[0].includes("missing"),
);

const stoppedKeys: string[] = [];
option.parse("clang", ["-fsyntax-only", "-Winvalid-stop-check"], (parseRes) => {
  if (typeof parseRes === "string") {
    throw new Error(`stop parse: unexpected parse error: ${parseRes}`);
  }
  stoppedKeys.push(parseRes.key);
  return false;
});
expectEq(stoppedKeys.length, 1, "stop parse length");
expectEq(stoppedKeys[0], "-fsyntax-only", "stop parse first key");

// a xmake demo
const command =
  "-o build/linux/x86_64/debug/catter build/.objs/catter/linux/x86_64/debug/src/catter/main.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/ipc.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/session.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/os.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/option.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/io.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/service.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/fs.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/apitool.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/js.cc.o build/.objs/catter-core/linux/x86_64/debug/api/output/lib/lib.js.o -m64 -L/home/kacent/.xmake/packages/q/quickjs-ng/v0.11.0/3b0b0541a046418183a839d92c0ee676/lib -Lbuild/linux/x86_64/debug -L/home/kacent/.xmake/packages/s/spdlog/v1.15.3/30816fde81524216904b4fedec0afba9/lib -L/home/kacent/.xmake/packages/k/kotatsu/66/633f0ffaa3c04216a51733d87ad6e471/lib -L/home/kacent/.xmake/packages/l/libuv/v1.52.0/36f98318201548a8ba03dcfff7683ae4/lib -L/home/kacent/.xmake/packages/c/cpptrace/v1.0.4/9e29ee9be85b4fd08157d98d4b9e2c49/lib -L/home/kacent/.xmake/packages/l/libdwarf/2.3.0/a4d27336f566462e80a33b85f1aec162/lib -L/home/kacent/.xmake/packages/z/zlib/v1.3.1/db68dfed70ca4c0b92a3b0b946951d79/lib -L/home/kacent/.xmake/packages/z/zstd/v1.5.7/bbc2fa368000410da812e97c13ccbbe3/lib -lqjs -lcommon -lspdlogd -lztest -loption -lasync -luv -lcpptrace -ldwarf -lz -lzstd -lm -lpthread -ldl -fsanitize=address".split(
    " ",
  );
const demoParsed: option.OptionItem[] = [];
option.parse("clang", command, (parseRes) => {
  debug.assertThrow(typeof parseRes !== "string");
  if (typeof parseRes !== "string") {
    demoParsed.push(parseRes);
  }
  return true;
});
debug.assertThrow(demoParsed.length > 10);
expectEq(
  option.stringify("clang", demoParsed[0]),
  "-o build/linux/x86_64/debug/catter",
  "stringify demo output",
);
const spdlogLink = demoParsed.find(
  (item) => option.stringify("clang", item) === "-lspdlogd",
);
debug.assertThrow(spdlogLink !== undefined);

// replace

const newCmd = option.replace(
  "clang",
  "-o build/linux/x86_64/debug/catter build/.objs/catter/linux/x86_64/debug/src/catter/main.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/ipc.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/session.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/os.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/option.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/io.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/service.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/capi/fs.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/apitool.cc.o build/.objs/catter-core/linux/x86_64/debug/src/catter/core/js/js.cc.o build/.objs/catter-core/linux/x86_64/debug/api/output/lib/lib.js.o -m64 -L/home/kacent/.xmake/packages/q/quickjs-ng/v0.11.0/f2160e7e0cf64779a62dd4ae96ef37e2/lib -Lbuild/linux/x86_64/debug -L/home/kacent/.xmake/packages/s/spdlog/v1.15.3/1a3282179c394ba089a988a4f35f26b0/lib -L/home/kacent/.xmake/packages/k/kotatsu/73/2189f84b06ee4d8fb12f20abf385213d/lib -L/home/kacent/.xmake/packages/l/libuv/v1.52.0/e0b29ff5583447e5833cc60c24aaa354/lib -L/home/kacent/.xmake/packages/c/cpptrace/v1.0.4/efd2fd5f2c774566a77fe4c35ab9ee2c/lib -L/home/kacent/.xmake/packages/l/libdwarf/2.3.0/1643c83c9f2b454ea5b223360a7abb8b/lib -L/home/kacent/.xmake/packages/z/zlib/v1.3.1/47f5907ebdb04479a4e2679f95ffa2b5/lib -L/home/kacent/.xmake/packages/z/zstd/v1.5.7/8b39fe4c8c5149d0a3747878b6e2dafc/lib -lqjs -lcommon -lspdlogd -lztest -loption -lasync -luv -lcpptrace -ldwarf -lz -lzstd -lm -lpthread -ldl -fsanitize=address".split(
    " ",
  ),
  (parseRes) => {
    debug.assertThrow(typeof parseRes !== "string");
    switch (
      option.convertToUnalias("clang", parseRes as option.OptionItem)
        .id as option.ClangID
    ) {
      case option.ClangID.ID_o:
        return "-o 233";
      case option.ClangID.ID_INPUT:
        return "<input>";
      case option.ClangID.ID_L:
        return "-L<...>";
      case option.ClangID.ID_fsanitize_EQ:
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

const nvccArgs = [
  "-c",
  "-Xcompiler",
  "-fPIE",
  "-Ihelper",
  "-I/usr/local/cuda/include",
  "-m64",
  "-ccbin=/home/.pixi/envs/default/bin/x86_64-conda-linux-gnu-c++",
  "-o",
  "build/.objs/sgemm/linux/x86_64/release/sgemm/main.cu.o",
  "sgemm/main.cu",
];

debug.assertThrow(JSON.stringify(cmd.nvcc2clang(nvccArgs)).includes("-ccbin="));
