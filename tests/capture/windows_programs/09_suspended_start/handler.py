import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "child" in command.args:
            return catter.Drop()
        return None


catter.serve(Handler())
