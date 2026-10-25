# A handler that speaks the protocol itself, without the SDK: it asks catter
# to analyze a program whose name is not base64, and a request catter does
# not know, prints the errors catter answers with, then serves the session.
import json
import sys


def write(message):
    body = json.dumps({"jsonrpc": "2.0", **message}).encode()
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
    sys.stdout.buffer.flush()


def read():
    length = 0
    while (line := sys.stdin.buffer.readline()) != b"\r\n":
        if not line:
            return None
        name, _, value = line.decode("ascii").partition(":")
        if name.lower() == "content-length":
            length = int(value)
    return json.loads(sys.stdin.buffer.read(length))


bad = {"programs": [{"executable": {"base64": "not base64!"}, "args": []}]}
write({"id": "bad", "method": "analyze", "params": bad})
write({"id": "unknown", "method": "hello", "params": {}})
answers = {}
while (message := read()) is not None:
    if "method" not in message:
        answers[message["id"]] = message["error"]
        continue
    if message["method"] == "initialize":
        events = {"command": False, "error": False, "exit": False}
        result = {"decides": False, "events": events, "env": False, "analysis": False}
        write({"id": message["id"], "result": result})
    elif message["method"] == "finish":
        for id in sorted(answers):
            print(f"{id}: {answers[id]['code']} {answers[id]['message']}", file=sys.stderr)
        write({"id": message["id"], "result": None})
