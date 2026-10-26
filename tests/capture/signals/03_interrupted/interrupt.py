# Runs the command on a terminal of its own, catter the one CATTER_EXECUTABLE
# names, and types Ctrl+C there once the build is ready, as a user stops a
# build: the terminal sends SIGINT to every process of its foreground group.
# Prints what the command wrote there, and how it ended.
import os
import pty
import signal
import sys
import termios

command = sys.argv[1:]
if command[0] == "catter":
    command[0] = os.environ["CATTER_EXECUTABLE"]
pid, terminal = pty.fork()
if pid == 0:
    # A terminal echoes ^C, and starts its programs with SIGINT and SIGHUP at
    # their defaults.
    attributes = termios.tcgetattr(0)
    attributes[3] &= ~(termios.ECHO | termios.ECHOCTL)
    termios.tcsetattr(0, termios.TCSANOW, attributes)
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    signal.signal(signal.SIGHUP, signal.SIG_DFL)
    os.execvp(command[0], command)

output = b""
typed = False
while True:
    try:
        data = os.read(terminal, 4096)
    except OSError:
        break
    if not data:
        break
    output += data
    if not typed and b"ready\r\n" in output:
        os.write(terminal, b"\x03")
        typed = True
_, status = os.waitpid(pid, 0)
print(output.decode().replace("\r\n", "\n"), end="")
name = sys.argv[1]
if os.WIFSIGNALED(status):
    print(f"{name} ended by {signal.Signals(os.WTERMSIG(status)).name}")
else:
    print(f"{name} ended by exit code {os.WEXITSTATUS(status)}")
