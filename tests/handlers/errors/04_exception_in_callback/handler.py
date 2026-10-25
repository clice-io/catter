import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        raise RuntimeError("raised from on_command")


catter.serve(Handler())
