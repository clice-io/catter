# Asks catter what it makes of each command it is told of, with the program
# swapped for clang, while catter goes on telling it of commands.
import catter


class Handler(catter.Handler):
    def on_command(self, command):
        [analysis] = self.analyze(catter.Program("clang", ["clang", *command.args[1:]]))
        print(f"command {command.id}: {getattr(analysis, 'phase', type(analysis).__name__)}")

    def on_finish(self, finish):
        print(f"finish: code {finish.code}")


catter.serve(Handler())
