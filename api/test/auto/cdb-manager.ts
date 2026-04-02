import { cmd, debug, fs, io } from "catter";

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function readJSON(path: string): unknown {
  let content = "";
  io.TextFileStream.with(path, "ascii", (stream) => {
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
];

debug.assertThrow(fs.createFile(inheritedPath));
io.TextFileStream.with(inheritedPath, "ascii", (stream) => {
  stream.write(JSON.stringify(inheritedItems, null, 2));
});

const manager = new cmd.CDBManager(inheritedPath);
const initialItems = manager.items();
expectEq(initialItems.length, 3, "initial item count");

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
expectEq(mergedItems.length, 4, "merged item count");
expectEq(mergedItems[0].file, "../src/keep.cc", "kept inherited file");
expectEq(
  mergedItems[1].command,
  "clang++ -Winvalid -c override.cc",
  "override command",
);
expectEq(mergedItems[1].output, "override-new.o", "override output");
expectEq(
  mergedItems[2].command,
  "clang++ -Winvalid -c override.cc -fmodules",
  "second override command",
);
expectEq(mergedItems[2].output, "override-mod.o", "second override output");
expectEq(mergedItems[3].file, "new.cc", "new item appended");
debug.assertThrow(Array.isArray(mergedItems[3].arguments));

const savePath = fs.path.joinAll(testEnvPath, "out", "compile_commands.json");
expectEq(manager.save(savePath), savePath, "save path");
debug.assertThrow(fs.exists(savePath));

const savedJSON = readJSON(savePath);
debug.assertThrow(Array.isArray(savedJSON));
if (!Array.isArray(savedJSON)) {
  throw new Error("saved cdb should be an array");
}
expectEq(savedJSON.length, 4, "saved item count");

const reloaded = new cmd.CDBManager(savePath).items();
expectEq(reloaded.length, 4, "reloaded item count");
expectEq(
  reloaded[1].command,
  "clang++ -Winvalid -c override.cc",
  "reloaded override command",
);
expectEq(
  reloaded[2].command,
  "clang++ -Winvalid -c override.cc -fmodules",
  "reloaded second override command",
);
