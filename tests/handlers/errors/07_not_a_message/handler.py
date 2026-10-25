# A handler that frames its messages itself, and gets it wrong. It runs
# until catter ends its input.
import sys

sys.stdout.buffer.write(b"Content-Length: many\r\n\r\n")
sys.stdout.buffer.flush()
sys.stdin.read()
