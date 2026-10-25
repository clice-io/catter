# A handler that speaks the protocol itself, without the SDK, and prints each
# message catter sends it: its method, whether it is a request, and the id of
# its command. It decides when given "decides", and else observes the
# commands and their exits.
import json
import sys

decides = sys.argv[1:] == ["decides"]


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
    kind = "request" if "id" in message else "notification"
    command = message.get("params", {}).get("id", "")
    print(f"{message['method']}: {kind} {command}".rstrip(), file=sys.stderr, flush=True)
    if message["method"] == "initialize":
        events = {"command": not decides, "error": False, "exit": not decides}
        answer(message, {"decides": decides, "events": events, "env": False})
    elif message["method"] == "command" and "id" in message:
        answer(message, {"action": "skip"})
    elif message["method"] == "finish":
        answer(message, None)
