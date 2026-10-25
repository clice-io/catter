# Asks catter what it makes of commands the handler names itself, once the
# session starts.
import catter


class Handler(catter.Handler):
    def on_start(self, session):
        analyses = self.analyze(
            catter.Program("gcc", ["gcc", "-c", "a.c", "-o", "a.o"]),
            catter.Program("clang", ["clang", "-c", "b.c", "-o"]),
            catter.Program("node", ["node", "-e", "0"]),
        )
        for analysis in analyses:
            if isinstance(analysis, catter.CompilerAnalysis):
                print(f"{analysis.compiler}: {analysis.phase}, writes {[w.path for w in analysis.writes]}")
            else:
                print(f"{type(analysis).__name__}: {getattr(analysis, 'error', None)}")


catter.serve(Handler())
