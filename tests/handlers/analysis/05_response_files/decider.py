# Edits the arguments a response file holds, or those around it.
import catter

EDITS = {
    2: [catter.CommandEdit("append", ["-DNEW"])],
    3: [catter.CommandEdit("remove", ["/Fo*"])],
}


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if command.id in EDITS:
            return catter.Edit(edits=EDITS[command.id])
        return None


catter.serve(Handler())
