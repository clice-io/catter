// Asks the server for the program it runs, as a build of later does.
import { readFileSync } from "node:fs";
import { connect } from "node:net";

const socket = connect(Number(readFileSync("port.txt", "utf8")), "127.0.0.1");
let answer = "";
socket.on("data", (chunk) => (answer += chunk));
socket.on("end", () => process.stdout.write(`server: ${answer}`));
