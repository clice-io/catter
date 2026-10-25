import catter


class Handler(catter.Handler):
    decides = True

    def on_exit(self, exit):
        if exit.code != 0:
            raise RuntimeError(f"command {exit.id} exited with {exit.code}")


catter.serve(Handler())
