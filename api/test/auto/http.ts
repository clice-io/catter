import { debug, http } from "catter";

const client = new http.Client();
client.close();
client.close();

let closedClientRejected = false;
try {
  await client.get("http://127.0.0.1:1/");
} catch (error) {
  closedClientRejected = String(error).includes("HTTP client is closed");
}

debug.assertThrow(closedClientRejected);
