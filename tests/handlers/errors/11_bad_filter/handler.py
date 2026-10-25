import catter


class Handler(catter.Handler):
    filter = [catter.Pattern(kind="compiler", phases=["compile", "assemble"])]

    def on_command(self, command):
        pass


catter.serve(Handler())
