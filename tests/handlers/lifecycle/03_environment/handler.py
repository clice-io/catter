import os

import catter


class Handler(catter.Handler):
    decides = True
    env = True

    def on_command(self, command):
        if "child" not in command.args:
            return None
        value = next(entry for entry in command.env if entry.startswith("CASE_VAR="))
        print(f"command {command.id} has {value}")
        return catter.Modify(
            args=command.args,
            cwd=os.path.join(command.cwd, "sub"),
            env=[*command.env, "CASE_ADDED=by-handler"],
        )


catter.serve(Handler())
