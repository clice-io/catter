# Runs a program with an argument that is not UTF-8, and prints what it
# received, byte for byte; then one given that argument in a directory the
# build removed, which catter cannot capture and names in its message.
import os
import subprocess
import sys

child = subprocess.run(
    [sys.executable, "child.py", b"caf\xe9"], capture_output=True, check=False
)
sys.stdout.write(child.stdout.decode("ascii"))

os.mkdir("gone")
os.chdir("gone")
os.rmdir("../gone")
subprocess.run([sys.executable, "-c", "0", b"caf\xe9"], check=False)
