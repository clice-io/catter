# Decides on the compiles and the linker's commands alone, and prints those it
# is asked about and their exits.
import catter


class Handler(catter.Handler):
    decides = True
    filter = [
        catter.Pattern(kind="compiler", phases=["compile"]),
        catter.Pattern(kind="tool", names=["lld-elf"]),
    ]

    def on_command(self, command):
        print(f"command {command.id}: {command.args}")

    def on_exit(self, exit):
        print(f"exit {exit.id}: code {exit.code}")


catter.serve(Handler())
