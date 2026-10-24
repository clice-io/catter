import { createHash } from "node:crypto";
import * as fs from "node:fs";

const data = fs.readFileSync(0);
console.log(`${data.length} bytes, sha256 ${createHash("sha256").update(data).digest("hex")}`);
