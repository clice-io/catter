import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if not command.args[-1].startswith("caf"):
            return None
        print(f"command {command.id} has {command.args[-1].encode('utf-8', 'surrogateescape')}")
        return catter.Modify(args=[*command.args, "caf\udce9-added"])


catter.serve(Handler())
