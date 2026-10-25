# Prints what catter makes of each command it is told of.
import catter


def argument(argument):
    values = "".join(f" {value!r}" for value in argument.values)
    return f"{argument.option} as {argument.spelling!r}{values} at {argument.index}+{argument.count}"


class Handler(catter.Handler):
    analysis = True

    def on_command(self, command):
        analysis = command.analysis
        print(f"command {command.id}: {type(analysis).__name__}")
        if isinstance(analysis, catter.CompilerAnalysis):
            print(f"  {analysis.compiler} in {analysis.dialect}: {analysis.phase} to {analysis.artifact}")
            target = analysis.target
            # The target a command does not name is the host's.
            if target.source != "host-fallback":
                print(
                    f"  for {target.triple}: {target.os} {target.objectFormat} {target.env},"
                    f" {target.artifactModel} from {target.source}"
                )
            for each in analysis.arguments:
                print(f"  argument {argument(each)}")
            for read in analysis.reads:
                print(
                    f"  reads {read.path} at {read.index} of argument {read.argument}"
                    f" for {read.role} in {read.language}"
                )
            for write in analysis.writes:
                print(f"  writes {write.path} from {write.reads}, {write.origin}")
            for diagnostic in analysis.diagnostics:
                print(
                    f"  diagnostic {diagnostic.code}: {diagnostic.path} at {diagnostic.index}"
                    f" of argument {diagnostic.argument}: {diagnostic.message}"
                )
        elif isinstance(analysis, catter.ArchiverAnalysis):
            thin = " thin" if analysis.thin else ""
            print(f"  {analysis.operation}{analysis.modifiers}{thin} {analysis.archive} of {analysis.members}")
            print(f"  reads {analysis.reads}, writes {analysis.writes}")
        elif isinstance(analysis, catter.ToolAnalysis):
            print(f"  {analysis.tool}")
            for each in analysis.arguments:
                print(f"  argument {argument(each)}")
        elif isinstance(analysis, catter.UnparsedAnalysis):
            print(f"  {analysis.of} {analysis.name}: {analysis.error}: {analysis.message}")


catter.serve(Handler())
