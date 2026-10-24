# A handler that frames its messages itself, and gets it wrong. It runs
# until catter ends its input.
import sys

sys.stdout.write("Content-Length: many\r\n\r\n")
sys.stdout.flush()
sys.stdin.read()
