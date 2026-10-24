# Decides after decider.py: it sees each command as decider.py left it, and
# prints that once the build is done.
import catter


class Handler(catter.Handler):
    decides = True

    def __init__(self):
        self.seen = []

    def on_command(self, command):
        self.seen.append(command.args[1:])
        if "leave-me" in command.args:
            return catter.Modify(args=[*command.args, "added-by-second"])
        return catter.Skip()

    def on_finish(self, finish):
        for args in self.seen:
            print(f"second saw {args}")


catter.serve(Handler())
