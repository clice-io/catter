import { debug, fs, io } from "catter";

// use pwd/res/fs-test-env as the test environment
const testEnvPath = fs.path.joinAll(".", "res", "fs-test-env");

const subDirs = fs.readDirs(testEnvPath).map((dir) => fs.path.absolute(dir));
const shouldBeSubDirs = ["a", "b", "c"].map((dir) =>
  fs.path.absolute(fs.path.joinAll(testEnvPath, dir)),
);

debug.assertThrow(
  shouldBeSubDirs.every((dir) => subDirs.includes(dir)) && subDirs.length === 3,
);

// also test io stream
const aTmpStream = new io.TextFileStream(
  fs.path.joinAll(testEnvPath, "a", "tmp.txt"),
);

// read
const entireBinary = aTmpStream.readLines();
debug.assertThrow(
  entireBinary.length === 4 &&
    entireBinary[0] === "Alpha!" &&
    entireBinary[1] === "Beta!" &&
    entireBinary[2] === "Kid A;" &&
    entireBinary[3] === "end;",
);
aTmpStream.close();

const endPat = /\r\n/g;
// write
io.TextFileStream.with(
  fs.path.joinAll(testEnvPath, "b", "tmp2.txt"),
  "ascii",
  (stream) => {
    stream.append("Appended line.\r\n");
    debug.assertThrow(
      stream.readEntireFile().replace(endPat, "\n") ===
        "Ok computer!\nAppended line.\n",
    );
  },
);

const largeTextPath = fs.path.joinAll(testEnvPath, "b", "large.txt");
const largeText = "A".repeat(70_000);
debug.assertThrow(fs.createFile(largeTextPath));
io.TextFileStream.with(largeTextPath, "ascii", (stream) => {
  stream.write(largeText);
});
io.TextFileStream.with(largeTextPath, "ascii", (stream) => {
  debug.assertThrow(stream.readEntireFile() === largeText);
});

const c_path = fs.path.joinAll(testEnvPath, "c");
debug.assertThrow(fs.exists(c_path));
debug.assertThrow(
  fs.readDirs(c_path).every((fname) => fs.path.extension(fname) === ".txt"),
);

// path raw
debug.assertThrow(fs.path.extension("a/n") === "");
debug.assertThrow(fs.path.filename("a/b/c.ext") === "c.ext");
debug.assertThrow(fs.path.filename("a/b/c") === "c");
debug.assertThrow(
  fs.path.lexicalNormal(fs.path.toAncestor("a/b/c/d/e.txt", 2)) ===
    fs.path.lexicalNormal(fs.path.joinAll("a", "b", "c")),
);
debug.assertThrow(
  fs.path.lexicalNormal(fs.path.relativeTo("a/b", "a/b/c/d/e")) ===
    fs.path.lexicalNormal(fs.path.joinAll("c", "d", "e")),
);
debug.assertThrow(
  fs.path.lexicalNormal("a/b/../c/./d") === fs.path.lexicalNormal("a/c/d"),
);

// create and remove dir/file recursively
const newFilePath = fs.path.joinAll(
  testEnvPath,
  "d",
  "e",
  "f",
  "g",
  "h",
  "i.txt",
);
debug.assertThrow(fs.createFile(newFilePath));
debug.assertThrow(fs.exists(newFilePath) && fs.isFile(newFilePath));

// create dirs
const newDirPath = fs.path.joinAll(testEnvPath, "x", "y", "z");
debug.assertThrow(fs.mkdir(newDirPath));
debug.assertThrow(fs.exists(newDirPath) && fs.isDir(newDirPath));

const asyncRoot = fs.path.joinAll(testEnvPath, "async");
const asyncTextPath = fs.path.joinAll(asyncRoot, "hello.txt");
const asyncRenamedPath = fs.path.joinAll(asyncRoot, "renamed.txt");
const asyncText = "Hello from async fs.\n" + "0123456789".repeat(8192);

debug.assertThrow(await fs.async.mkdir(asyncRoot));
await fs.async.writeText(asyncTextPath, asyncText);
debug.assertThrow(await fs.async.exists(asyncTextPath));
debug.assertThrow(await fs.async.isFile(asyncTextPath));
debug.assertThrow((await fs.async.readText(asyncTextPath)) === asyncText);

const asyncEntries = await fs.async.readDirs(asyncRoot);
debug.assertThrow(
  asyncEntries.map((entry) => fs.path.filename(entry)).includes("hello.txt"),
);

debug.assertThrow(await fs.async.rename(asyncTextPath, asyncRenamedPath));
debug.assertThrow(!(await fs.async.exists(asyncTextPath)));
debug.assertThrow(await fs.async.exists(asyncRenamedPath));

await fs.async.removeAll(asyncRoot);
debug.assertThrow(!(await fs.async.exists(asyncRoot)));
