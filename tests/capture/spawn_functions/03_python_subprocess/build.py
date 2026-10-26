# Starts programs as Python build tools (Meson, SCons, waf) do, through
# subprocess, whose child closes every descriptor it is not given, a log file
# of catter's hook among them, before it runs the program: on Linux the child
# of a vfork, which shares its parent's memory. One after another, then one
# that does not exist.
import subprocess
import sys

for i in range(16):
    run = subprocess.run([sys.executable, "-c", f"print('n{i}')"], capture_output=True, text=True)
    print(run.stdout.strip(), "exit", run.returncode)
try:
    subprocess.run(["catter-test-no-such-program"])
except FileNotFoundError as error:
    print("failed to start:", error.strerror)
