import shutil

import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "act-on-me" in command.args:
            return catter.Modify(
                executable=shutil.which("python"),
                args=["python", "-c", "print('python ran in its place')"],
            )
        return None


catter.serve(Handler())
