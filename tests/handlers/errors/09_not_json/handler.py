# A handler that frames its messages itself, and frames what is not JSON. It
# runs until catter ends its input.
import sys

sys.stdout.buffer.write(b"Content-Length: 1\r\n\r\nx")
sys.stdout.buffer.flush()
sys.stdin.read()
