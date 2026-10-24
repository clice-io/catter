# Prints, once the build is done, the arguments of each command it was told
# of.
import catter


class Handler(catter.Handler):
    def __init__(self):
        self.seen = []

    def on_command(self, command):
        self.seen.append(command.args[1:])

    def on_finish(self, finish):
        for args in self.seen:
            print(f"observed {args}")


catter.serve(Handler())
