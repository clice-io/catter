import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "drop-me" in command.args:
            return catter.Drop()
        if "modify-me" in command.args:
            return catter.Modify(args=[*command.args, "added-by-decider"])
        return None


catter.serve(Handler())
