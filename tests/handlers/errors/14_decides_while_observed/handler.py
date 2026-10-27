import catter


class Handler(catter.Handler):
    decides = True

    def on_start(self, session):
        print(f"start: mode {session.mode}, decisions {session.decisions}")


catter.serve(Handler())
