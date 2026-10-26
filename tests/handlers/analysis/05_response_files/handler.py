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
            # A program the handler names has its relative response files read from its cwd, if any.
            args = ["gcc", "@flags.rsp", "-c", "a.c"]
            here, nowhere = self.analyze(catter.Program("gcc", args, command.cwd), catter.Program("gcc", args))
            print(f"  gcc @flags.rsp -c a.c, in the command's cwd: as {here.expansion.args}")
            print(f"  gcc @flags.rsp -c a.c, in no cwd: {[each.code for each in nowhere.diagnostics]}")


catter.serve(Handler())
