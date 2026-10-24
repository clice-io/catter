import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "act-on-me" in command.args:
            return catter.Abort(reason="act-on-me must not run")
        return None


catter.serve(Handler())
