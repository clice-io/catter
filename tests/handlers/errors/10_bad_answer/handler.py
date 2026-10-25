# A handler that speaks the protocol itself, without the SDK, and answers
# the first command with what is given to it: an action catter does not know,
# or an argument whose base64 is wrong.
import json
import sys

actions = {
    "unknown": {"action": "rerun"},
    "base64": {"action": "modify", "args": ["node", {"base64": "not base64!"}]},
}
action = actions[sys.argv[1]]


def read():
    length = 0
    while (line := sys.stdin.buffer.readline()) != b"\r\n":
        if not line:
            return None
        name, _, value = line.decode("ascii").partition(":")
        if name.lower() == "content-length":
            length = int(value)
    return json.loads(sys.stdin.buffer.read(length))


def answer(message, result):
    body = json.dumps({"jsonrpc": "2.0", "id": message["id"], "result": result}).encode()
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
    sys.stdout.buffer.flush()


while (message := read()) is not None:
    if message["method"] == "initialize":
        answer(message, {"decides": True, "events": {"command": False, "error": False, "exit": False}, "env": False})
    elif message["method"] == "command":
        answer(message, action)
