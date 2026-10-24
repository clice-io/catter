import threading

import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "first" in command.args:
            # Never answers.
            threading.Event().wait()
        return None


catter.serve(Handler())
