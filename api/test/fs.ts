import { debug, fs, io } from "catter";

// use pwd/res/fs-test-env as the test environment
const testEnvPath = fs.path.joinAll(".", "res", "fs-test-env");

const subDirs = fs.readDirs(testEnvPath).map((dir) => fs.path.absolute(dir));
const shouldBeSubDirs = ["a", "b", "c"].map((dir) =>
  fs.path.absolute(fs.path.joinAll(testEnvPath, dir)),
);

debug.assert_throw(
  shouldBeSubDirs.every((dir) => subDirs.includes(dir)) && subDirs.length === 3,
);

// also test io stream
const aTmpStream = new io.TextFileStream(
  fs.path.joinAll(testEnvPath, "a", "tmp.txt"),
);

const entireBinary = aTmpStream.readLines();
debug.assert_throw(
  entireBinary.length === 4 &&
    entireBinary[0] === "Alpha!" &&
    entireBinary[1] === "Beta!" &&
    entireBinary[2] === "Kid A;" &&
    entireBinary[3] === "end;",
);
aTmpStream.close();

// write
io.TextFileStream.with(
  fs.path.joinAll(testEnvPath, "b", "tmp2.txt"),
  "ascii",
  (stream) => {
    stream.append("Appended line.\n");
    debug.assert_throw(
      stream.readEntireFile() === "Ok computer!Appended line.\n",
    );
  },
);

const c_path = fs.path.joinAll(testEnvPath, "c");
debug.assert_throw(fs.exists(c_path));
debug.assert_throw(
  fs.readDirs(c_path).every((fname) => fs.path.extension(fname) === ".txt"),
);

// path raw
debug.assert_throw(fs.path.extension("a/n") === "");
debug.assert_throw(fs.path.filename("a/b/c.ext") === "c.ext");
debug.assert_throw(fs.path.filename("a/b/c") === "c");
debug.assert_throw(
  fs.path.to_ancestor("a/b/c/d/e.txt", 2) === fs.path.joinAll("a", "b", "c"),
);
