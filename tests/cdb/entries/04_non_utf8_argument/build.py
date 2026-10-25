# Compiles a source with a define whose value is not UTF-8, as a build passes
# on bytes it read from a file.
import subprocess
import sys

compile = [b"c++", b"-c", b"main.cc", b"-o", b"main.o", b"-DNAME=caf\xe9"]
sys.exit(subprocess.run(compile, check=False).returncode)
