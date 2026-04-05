import { cmd, debug, fs, io } from "catter";

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function expectDefined<T>(value: T | undefined, label: string): T {
  if (value === undefined) {
    throw new Error(`${label}: expected value`);
  }
  return value;
}

function readJSON(path: string): unknown {
  let content = "";
  io.TextFileStream.with(path, "utf-8", (stream) => {
    content = stream.readEntireFile();
  });
  return JSON.parse(content);
}

const testEnvPath = fs.path.joinAll(".", "cdb-manager-test-env");
if (fs.exists(testEnvPath)) {
  fs.removeAll(testEnvPath);
}
debug.assertThrow(fs.mkdir(testEnvPath));

const buildDir = fs.path.joinAll(testEnvPath, "build");
const sourceDir = fs.path.joinAll(testEnvPath, "src");
const inheritedPath = fs.path.joinAll(testEnvPath, "compile_commands.json");

const inheritedItems = [
  {
    directory: buildDir,
    file: "../src/keep.cc",
    command: "clang++ -c ../src/keep.cc",
  },
  {
    directory: buildDir,
    file: "override.cc",
    arguments: ["clang++", "-c", "override.cc"],
    output: "override.o",
  },
  {
    directory: buildDir,
    file: "override.cc",
    arguments: ["clang++", "-c", "override.cc", "-DFROM_OLD"],
    output: "override-old.o",
  },
  {
    directory: buildDir,
    file: fs.path.joinAll(sourceDir, "绝对路径.cc"),
    arguments: ["clang++", "-c", "绝对路径.cc", "-DNAME=你好"],
    output: "unicode.o",
  },
];

debug.assertThrow(fs.createFile(inheritedPath));
io.TextFileStream.with(inheritedPath, "utf-8", (stream) => {
  stream.write(JSON.stringify(inheritedItems, null, 2));
});

const manager = new cmd.CDBManager(inheritedPath);
const initialItems = manager.items();
expectEq(initialItems.length, 4, "initial item count");

const extraPath = fs.path.joinAll(testEnvPath, "other.json");
const extra = new cmd.CDBManager(extraPath);
extra.addItem({
  directory: fs.path.joinAll(buildDir, "."),
  file: "./override.cc",
  command: "clang++ -Winvalid -c override.cc",
  output: "override-new.o",
});
extra.addItem({
  directory: fs.path.joinAll(buildDir, "."),
  file: "./override.cc",
  command: "clang++ -Winvalid -c override.cc -fmodules",
  output: "override-mod.o",
});
extra.addItem({
  directory: sourceDir,
  file: "new.cc",
  arguments: ["clang++", "-c", "new.cc"],
});

manager.merge(extra);

const mergedItems = manager.items();
expectEq(mergedItems.length, 5, "merged item count");
const keptItem = expectDefined(
  mergedItems.find((item) => item.file === "../src/keep.cc"),
  "kept inherited item",
);
expectEq(keptItem.file, "../src/keep.cc", "kept inherited file");
const overrideItem = expectDefined(
  mergedItems.find((item) => item.output === "override-new.o"),
  "override item",
);
expectEq(
  overrideItem.command,
  "clang++ -Winvalid -c override.cc",
  "override command",
);
expectEq(overrideItem.output, "override-new.o", "override output");
const modulesItem = expectDefined(
  mergedItems.find((item) => item.output === "override-mod.o"),
  "second override item",
);
expectEq(
  modulesItem.command,
  "clang++ -Winvalid -c override.cc -fmodules",
  "second override command",
);
expectEq(modulesItem.output, "override-mod.o", "second override output");
const unicodeItem = expectDefined(
  mergedItems.find((item) => item.output === "unicode.o"),
  "unicode item",
);
expectEq(
  unicodeItem.file,
  fs.path.joinAll(sourceDir, "绝对路径.cc"),
  "absolute unicode file preserved",
);
debug.assertThrow(Array.isArray(unicodeItem.arguments));
expectEq(unicodeItem.arguments?.[3], "-DNAME=你好", "unicode flag preserved");
const newItem = expectDefined(
  mergedItems.find((item) => item.file === "new.cc"),
  "new item",
);
expectEq(newItem.file, "new.cc", "new item appended");
debug.assertThrow(Array.isArray(newItem.arguments));

const savePath = fs.path.joinAll(testEnvPath, "out", "compile_commands.json");
expectEq(manager.save(savePath), savePath, "save path");
debug.assertThrow(fs.exists(savePath));

const savedJSON = readJSON(savePath);
debug.assertThrow(Array.isArray(savedJSON));
if (!Array.isArray(savedJSON)) {
  throw new Error("saved cdb should be an array");
}
expectEq(savedJSON.length, 5, "saved item count");

const reloaded = new cmd.CDBManager(savePath).items();
expectEq(reloaded.length, 5, "reloaded item count");
const reloadedOverrideItem = expectDefined(
  reloaded.find((item) => item.output === "override-new.o"),
  "reloaded override item",
);
expectEq(
  reloadedOverrideItem.command,
  "clang++ -Winvalid -c override.cc",
  "reloaded override command",
);
const reloadedModulesItem = expectDefined(
  reloaded.find((item) => item.output === "override-mod.o"),
  "reloaded second override item",
);
expectEq(
  reloadedModulesItem.command,
  "clang++ -Winvalid -c override.cc -fmodules",
  "reloaded second override command",
);
const reloadedUnicodeItem = expectDefined(
  reloaded.find((item) => item.output === "unicode.o"),
  "reloaded unicode item",
);
expectEq(
  reloadedUnicodeItem.file,
  fs.path.joinAll(sourceDir, "绝对路径.cc"),
  "reloaded absolute unicode file",
);
expectEq(
  reloadedUnicodeItem.arguments?.[3],
  "-DNAME=你好",
  "reloaded unicode flag",
);
