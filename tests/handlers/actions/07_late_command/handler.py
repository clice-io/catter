import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        # The program the static one starts, not the build command, which
        # names the tag too.
        if command.args[:1] != ["node"]:
            return None
        if "act-on-me" in command.args:
            return catter.Modify(args=[*command.args, "added-by-handler"])
        if "drop-me" in command.args:
            return catter.Drop()
        return None


catter.serve(Handler())
