# Counts the programs build.mjs starts, which start at once: the order
# catter sees them in is not pinned, their number is. It decides on each, but
# given `observe`.
import sys

import catter


class Handler(catter.Handler):
    decides = sys.argv[1:] != ["observe"]

    def __init__(self):
        self.seen = set()

    def on_command(self, command):
        if "-e" in command.args:
            self.seen.add(command.args[-1])

    def on_finish(self, finish):
        print(f"catter saw {len(self.seen)} programs")


catter.serve(Handler())
