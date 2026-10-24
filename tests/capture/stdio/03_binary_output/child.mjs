import * as fs from "node:fs";

const bytes = Buffer.from(Array.from({ length: 256 }, (_, i) => i));
fs.writeSync(1, Buffer.concat(Array(4096).fill(bytes)));
fs.writeSync(2, Buffer.concat(Array(1024).fill(bytes.reverse())));
