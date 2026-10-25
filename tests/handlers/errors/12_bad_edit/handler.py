# Edits a command the way catter cannot: given "input", it removes an input,
# which names no option; given "program", it edits a program catter reads no
# arguments of.
import sys

import catter

wrong = sys.argv[1]


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if wrong == "program" and command.args[0] == "make":
            return catter.Edit(edits=[catter.CommandEdit("append", ["-j4"])])
        if wrong == "input" and command.args[0] == "gcc":
            return catter.Edit(edits=[catter.CommandEdit("remove", ["a.c"])])
        return None


catter.serve(Handler())
