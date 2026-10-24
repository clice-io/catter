# Prints what each callback receives, on stderr: serve() points what the
# handler prints there.
import os

import catter


class Handler(catter.Handler):
    def on_start(self, session):
        cwd = os.path.relpath(session.build.cwd)
        print(
            f"start: version {session.version}, mode {session.mode}, "
            f"build {session.build.command} in {cwd}, session of {len(session.session)} characters"
        )

    def on_command(self, command):
        print(
            f"command {command.id}: parent {command.parent}, args {command.args[1:]}, "
            f"in {os.path.relpath(command.cwd)}, env {command.env}"
        )

    def on_error(self, error):
        print(f"error {error.id}: parent {error.parent}, {error.message}")

    def on_exit(self, exit):
        print(f"exit {exit.id}: code {exit.code}, took {exit.duration // 1000}ms")

    def on_finish(self, finish):
        print(f"finish: code {finish.code}")


catter.serve(Handler())
print("served")
