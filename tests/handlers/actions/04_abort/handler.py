import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        print(f"command {command.id}: {command.args[1:]}")
        if "act-on-me" in command.args:
            return catter.Abort(reason="act-on-me must not run")
        return None

    def on_exit(self, exit):
        print(f"exit {exit.id}: code {exit.code}")

    def on_finish(self, finish):
        print(f"finish: code {finish.code}")


catter.serve(Handler())
