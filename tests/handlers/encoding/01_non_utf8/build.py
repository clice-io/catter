# Runs a program with an argument that is not UTF-8, and prints what it
# received, byte for byte.
import subprocess
import sys

child = subprocess.run(
    [sys.executable, "child.py", b"caf\xe9"], capture_output=True, check=False
)
sys.stdout.write(child.stdout.decode("ascii"))
