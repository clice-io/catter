import catter

REPLACED = "caf�"


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if not command.args[-1].startswith("caf") or command.args[0] == "./broken":
            return None
        print(f"command {command.id} has {command.args[-1].encode('utf-8', 'surrogateescape')}")
        return catter.Modify(args=[*command.args, "caf\udce9-added"])

    def on_error(self, error):
        print(f"error {error.id} has {REPLACED!r}: {REPLACED in error.message}")


catter.serve(Handler())
