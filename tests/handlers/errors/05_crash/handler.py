import os

import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "second" in command.args:
            os._exit(3)
        return None


catter.serve(Handler())
