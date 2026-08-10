import { assertThrow } from "catter/debug";
import { Client } from "catter/http";

const client = new Client();
client.close();
client.close();

let closedClientRejected = false;
try {
  await client.get("http://127.0.0.1:1/");
} catch (error) {
  closedClientRejected = String(error).includes("HTTP client is closed");
}

assertThrow(closedClientRejected);
