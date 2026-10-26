# Prints each command as the handlers before it left it.
import catter


class Handler(catter.Handler):
    def on_command(self, command):
        print(f"command {command.id}: {' '.join(command.args)}")


catter.serve(Handler())
