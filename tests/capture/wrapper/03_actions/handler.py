import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "drop.c" in command.args:
            return catter.Drop()
        if "modify.c" in command.args:
            return catter.Modify(args=[arg.replace("modify", "modified") for arg in command.args])
        if "abort.c" in command.args:
            return catter.Abort(reason="abort.c must not run")
        return None


catter.serve(Handler())
