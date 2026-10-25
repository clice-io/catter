# Counts the programs build.mjs starts, which start at once: the order
# catter sees them in is not pinned, their number is.
import catter


class Handler(catter.Handler):
    def __init__(self):
        self.seen = set()

    def on_command(self, command):
        if "-e" in command.args:
            self.seen.add(command.args[-1])

    def on_finish(self, finish):
        print(f"catter saw {len(self.seen)} programs")


catter.serve(Handler())
