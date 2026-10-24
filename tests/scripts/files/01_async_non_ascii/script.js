// Makes, lists and removes files with the async functions of catter/fs,
// under a directory named past ASCII, in a copy whose root is too.
import { println } from "catter/io";
import { onFinish } from "catter/service";
import {
  createFile,
  existsSync,
  mkdir,
  path,
  readDirs,
  removeAll,
} from "catter/fs";

onFinish(async () => {
  await mkdir("made/中文-é/dir");
  await createFile("made/中文-é/file.txt");
  const names = (await readDirs("made/中文-é")).map((entry) =>
    path.filename(entry),
  );
  println(`listed: ${names.sort().join(", ")}`);
  println(`file exists: ${existsSync("made/中文-é/file.txt")}`);
  await removeAll("made");
  println(`removed: ${!existsSync("made")}`);
});
