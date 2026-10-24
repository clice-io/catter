import { basename } from "node:path";

console.log(`ran in ${basename(process.cwd())} with ${JSON.stringify(process.argv.slice(2))}`);
console.log(`CATTER_CASE_VAR=${process.env.CATTER_CASE_VAR}`);
process.exitCode = 3;
