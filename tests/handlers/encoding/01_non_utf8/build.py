# Runs a program with an argument that is not UTF-8, and prints what it
# received, byte for byte; then a program that fails to start, given that
# argument, which catter reports with it in its message.
import os
import subprocess
import sys

child = subprocess.run(
    [sys.executable, "child.py", b"caf\xe9"], capture_output=True, check=False
)
sys.stdout.write(child.stdout.decode("ascii"))

with open("broken", "w") as script:
    script.write("#!/nonexistent/interpreter\n")
os.chmod("broken", 0o755)
subprocess.run(["./broken", b"caf\xe9"], check=False)
