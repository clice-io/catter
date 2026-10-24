import * as fs from "node:fs";

const o = Buffer.alloc(65536, "o");
const e = Buffer.alloc(65536, "e");
for (let i = 0; i < 64; i++) {
  fs.writeSync(1, o);
  fs.writeSync(2, e);
}
