import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if command.args[-1].startswith("caf"):
            return catter.Abort(reason=f"{command.args[-1]} must not run")
        return None


catter.serve(Handler())
