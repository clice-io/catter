import os
import sys

print(f"ran with {[os.fsencode(arg) for arg in sys.argv[1:]]}")
