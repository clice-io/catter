# Edits the arguments of every command catter reads the arguments of.
import catter

EDITS = {
    "gcc": [
        catter.CommandEdit("remove", ["-DOLD", "-W*"]),
        catter.CommandEdit("append", ["-DNEW=1", "-I", "generated"]),
        catter.CommandEdit("prepend", ["-fdiagnostics-color"]),
    ],
    "msvc": [catter.CommandEdit("append", ["/DNEW"])],
    "lld-elf": [catter.CommandEdit("remove", ["-l"]), catter.CommandEdit("append", ["--gc-sections"])],
    "clang": [catter.CommandEdit("append", ["-DNEW", "-DGONE"]), catter.CommandEdit("remove", ["-DGONE"])],
}


class Handler(catter.Handler):
    decides = True
    analysis = True

    def on_command(self, command):
        analysis = command.analysis
        name = getattr(analysis, "compiler", None) or getattr(analysis, "tool", None)
        if name in EDITS:
            return catter.Edit(edits=EDITS[name])
        return None


catter.serve(Handler())
