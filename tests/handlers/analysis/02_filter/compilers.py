# Observes every command of clang's, whose arguments read or not.
import catter


class Handler(catter.Handler):
    filter = [catter.Pattern(kind="compiler", names=["clang"])]

    def on_command(self, command):
        print(f"command {command.id}: {command.args}")


catter.serve(Handler())
