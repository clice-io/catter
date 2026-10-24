import catter


class Handler(catter.Handler):
    def on_start(self, session):
        print(f"started for {session.build.command}")


catter.serve(Handler())
