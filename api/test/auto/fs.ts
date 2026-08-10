import { assertThrow } from "catter/debug";
import {
  createFile,
  createFileSync,
  exists,
  existsSync,
  isDir,
  isDirSync,
  isFile,
  isFileSync,
  mkdir,
  mkdirSync,
  path,
  readDirs,
  readDirsSync,
  readText,
  removeAll,
  rename,
  writeText,
} from "catter/fs";
import { TextFileStream } from "catter/io";

// use pwd/res/fs-test-env as the test environment
const testEnvPath = path.joinAll(".", "res", "fs-test-env");

const subDirs = readDirsSync(testEnvPath).map((dir) => path.absolute(dir));
const shouldBeSubDirs = ["a", "b", "c"].map((dir) =>
  path.absolute(path.joinAll(testEnvPath, dir)),
);

assertThrow(
  shouldBeSubDirs.every((dir) => subDirs.includes(dir)) && subDirs.length === 3,
);

// also test io stream
const aTmpStream = new TextFileStream(
  path.joinAll(testEnvPath, "a", "tmp.txt"),
);

// read
const entireBinary = aTmpStream.readLines();
assertThrow(
  entireBinary.length === 4 &&
    entireBinary[0] === "Alpha!" &&
    entireBinary[1] === "Beta!" &&
    entireBinary[2] === "Kid A;" &&
    entireBinary[3] === "end;",
);
aTmpStream.close();

const endPat = /\r\n/g;
// write
TextFileStream.with(
  path.joinAll(testEnvPath, "b", "tmp2.txt"),
  "ascii",
  (stream) => {
    stream.append("Appended line.\r\n");
    assertThrow(
      stream.readEntireFile().replace(endPat, "\n") ===
        "Ok computer!\nAppended line.\n",
    );
  },
);

const largeTextPath = path.joinAll(testEnvPath, "b", "large.txt");
const largeText = "A".repeat(70_000);
assertThrow(createFileSync(largeTextPath));
TextFileStream.with(largeTextPath, "ascii", (stream) => {
  stream.write(largeText);
});
TextFileStream.with(largeTextPath, "ascii", (stream) => {
  assertThrow(stream.readEntireFile() === largeText);
});

const c_path = path.joinAll(testEnvPath, "c");
assertThrow(existsSync(c_path));
assertThrow(
  readDirsSync(c_path).every((fname) => path.extension(fname) === ".txt"),
);

// path raw
assertThrow(path.extension("a/n") === "");
assertThrow(path.filename("a/b/c.ext") === "c.ext");
assertThrow(path.filename("a/b/c") === "c");
assertThrow(
  path.lexicalNormal(path.toAncestor("a/b/c/d/e.txt", 2)) ===
    path.lexicalNormal(path.joinAll("a", "b", "c")),
);
assertThrow(
  path.lexicalNormal(path.relativeTo("a/b", "a/b/c/d/e")) ===
    path.lexicalNormal(path.joinAll("c", "d", "e")),
);
assertThrow(path.lexicalNormal("a/b/../c/./d") === path.lexicalNormal("a/c/d"));

// create and remove dir/file recursively
const newFilePath = path.joinAll(testEnvPath, "d", "e", "f", "g", "h", "i.txt");
assertThrow(createFileSync(newFilePath));
assertThrow(existsSync(newFilePath) && isFileSync(newFilePath));

// create dirs
const newDirPath = path.joinAll(testEnvPath, "x", "y", "z");
assertThrow(mkdirSync(newDirPath));
assertThrow(existsSync(newDirPath) && isDirSync(newDirPath));

const asyncRoot = path.joinAll(testEnvPath, "async");
const asyncTextPath = path.joinAll(asyncRoot, "hello.txt");
const asyncRenamedPath = path.joinAll(asyncRoot, "renamed.txt");
const asyncText = "Hello from async fs.\n" + "0123456789".repeat(8192);

assertThrow(await mkdir(asyncRoot));
await writeText(asyncTextPath, asyncText);
assertThrow(await exists(asyncTextPath));
assertThrow(await isFile(asyncTextPath));
assertThrow((await readText(asyncTextPath)) === asyncText);

const asyncEntries = await readDirs(asyncRoot);
assertThrow(
  asyncEntries.map((entry) => path.filename(entry)).includes("hello.txt"),
);

assertThrow(await rename(asyncTextPath, asyncRenamedPath));
assertThrow(!(await exists(asyncTextPath)));
assertThrow(await exists(asyncRenamedPath));

await removeAll(asyncRoot);
assertThrow(!(await exists(asyncRoot)));
