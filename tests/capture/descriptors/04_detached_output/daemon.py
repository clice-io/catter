import os
import sys

os.write(1, b"running\n")
os.close(1)
sys.stdin.read()
