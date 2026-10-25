# Observes the archiver's commands and lld-link's.
import catter


class Handler(catter.Handler):
    filter = [
        catter.Pattern(kind="archiver"),
        catter.Pattern(kind="tool", names=["lld-coff"]),
    ]

    def on_command(self, command):
        print(f"command {command.id}: {command.args}")


catter.serve(Handler())
