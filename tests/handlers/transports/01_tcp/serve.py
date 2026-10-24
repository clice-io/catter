# Listens on a port of this machine, starts catter connected to it, and
# serves catter's session there, as a handler run apart from catter does.
# What it and catter print shows once catter is done, one after the other.
import os
import subprocess
import sys

import catter


class Handler(catter.Handler):
    decides = True

    def on_start(self, session):
        print(f"start: mode {session.mode}")

    def on_command(self, command):
        print(f"command {command.id}: {command.args[1:]}")
        if "act-on-me" in command.args:
            return catter.Modify(args=[*command.args, "added-over-tcp"])
        return None

    def on_finish(self, finish):
        print(f"finish: code {finish.code}")


with catter.listen() as listener:
    build = subprocess.Popen(
        [
            os.environ["CATTER_EXECUTABLE"],
            "--connect",
            f"127.0.0.1:{listener.port}",
            "--",
            "node",
            "build.mjs",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    listener.accept(Handler())
    stdout, stderr = build.communicate()
    print(f"catter exited with {build.returncode}, printing:", flush=True)
    sys.stderr.buffer.write(stdout + stderr)
