# Prints what catter reads of each compile's response files.
import catter


class Handler(catter.Handler):
    analysis = True

    def on_command(self, command):
        analysis = command.analysis
        if not isinstance(analysis, catter.CompilerAnalysis):
            return
        print(f"command {command.id}: {' '.join(command.args)}")
        if analysis.expansion is not None:
            print(f"  reads {analysis.expansion.files}, as {analysis.expansion.args}")
        for read in analysis.reads:
            print(f"  reads {read.path} at {read.index}")
        for write in analysis.writes:
            print(f"  writes {write.path}")
        for diagnostic in analysis.diagnostics:
            print(f"  diagnostic {diagnostic.code}: {diagnostic.path} at {diagnostic.index}: {diagnostic.message}")
        if command.id == 4:
            # A program the handler names reads its response files from catter's working directory.
            (named,) = self.analyze(catter.Program("gcc", ["gcc", "@flags.rsp", "-c", "a.c"]))
            print(f"  gcc @flags.rsp -c a.c: as {named.expansion.args}")


catter.serve(Handler())
