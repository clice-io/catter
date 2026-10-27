import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if command.executable.lower().endswith("cl.exe"):
            return catter.Drop()
        return None


catter.serve(Handler())
