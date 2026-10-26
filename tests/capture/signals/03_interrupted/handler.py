# Says how the build ended, on stderr: serve() points what the handler prints
# there.
import catter


class Handler(catter.Handler):
    def on_finish(self, finish):
        print(f"the handler was told the build ended with {finish.code}")


catter.serve(Handler())
