# A handler that frames its messages itself, and sends catter a notification,
# which catter takes from no handler. It runs until catter ends its input.
import sys

body = b'{"jsonrpc":"2.0","method":"hello"}'
sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
sys.stdout.buffer.flush()
sys.stdin.read()
