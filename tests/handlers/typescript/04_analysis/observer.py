# Prints each command as the handlers before it left it, once the build is
# done: after the handler before it, which prints as commands come.
import catter


class Handler(catter.Handler):
    def __init__(self):
        self.seen = []

    def on_command(self, command):
        self.seen.append(f"seen {command.id}: {' '.join(command.args)}")

    def on_finish(self, finish):
        print("\n".join(self.seen))


catter.serve(Handler())
