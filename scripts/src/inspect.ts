import { cli, run } from "catter/cli";
import { create, register, type CatterContextService } from "catter/service";
import { println } from "catter/io";
import { path } from "catter/fs";
import { cdbItemsOf, type CDBItem } from "catter/cdb";
import {
    ArchiverAnalyzer,
    CompilerAnalyzer,
    CompilerIdentifier,
    CompilerResolver,
    Registry,
    unwrapCompilerCommand,
    type AnalyzedData,
    type CommandAnalysis,
    type CompilerAnalysis,
    type CommandAnalyzerError,
} from "catter/cmd";

const inspectCLI = cli.command({
    name: "inspect",
    description: "Print how catter analyzes the command after --, without running it.",
    examples: [
        {
            command: "inspect -- clang++ -c src/main.cc -o main.o",
            description: "The compiler, its mode, target, sources and outputs.",
        },
    ],
});

function nonEmpty<T>(values: readonly T[] | undefined): readonly T[] | undefined {
    return values === undefined || values.length === 0 ? undefined : values;
}

/// `file` as script::cdb records an output: absolute, from `cwd`.
function absolute(cwd: string, file: string): string {
    return path.lexicalNormal(
        path.isAbsolute(file) ? file : path.joinAll(path.absolute(cwd), file),
    );
}

/// The entries script::cdb writes for this command alone: one per source and
/// output made from it, but for the standard streams (`-`).
function entriesOf(cwd: string, command: AnalyzedData, analysis: CompilerAnalysis): CDBItem[] {
    const sources = new Set(analysis.sourceFiles);
    sources.delete("-");
    return analysis.edges
        .filter((edge) => edge.output !== "-")
        .flatMap((edge) =>
            cdbItemsOf(
                { cwd, argv: [...command.argv] },
                edge.inputs
                    .filter((input) => sources.has(input))
                    .map((file) => ({ file, output: absolute(cwd, edge.output) })),
            ),
        );
}

/** What catter makes of one command: the fields every analysis has, then those of its kind. */
function describe(
    cwd: string,
    command: AnalyzedData,
    analysis: CommandAnalysis,
): Record<string, unknown> {
    const effects = {
        reads: analysis.reads,
        writes: analysis.writes,
        edges: analysis.edges,
    };
    if (analysis.kind === "archiver") {
        return {
            analyzer: analysis.kind,
            operation: analysis.operation,
            modifiers: analysis.modifiers,
            thin: analysis.thin,
            archive: analysis.archive,
            members: analysis.members,
            ...effects,
        };
    }
    const identity = new CompilerIdentifier().identifyCompilerCommand(
        unwrapCompilerCommand(command),
    );
    return {
        analyzer: analysis.kind,
        compiler: identity.key,
        dialect: identity.dialect,
        unwrapped:
            analysis.unwrappedArgv.join("\0") === command.argv.join("\0")
                ? undefined
                : analysis.unwrappedArgv,
        phase: analysis.compilerMode.phase,
        artifact: analysis.compilerMode.artifact,
        target: {
            ...analysis.target.descriptor,
            artifactModel: analysis.target.artifactModel,
            from: analysis.target.source.kind,
        },
        sources: analysis.sourceFiles,
        ...effects,
        inferred: nonEmpty(analysis.debug?.inferredWrites),
        diagnostics: nonEmpty(analysis.debug?.diagnostics),
        entries: entriesOf(cwd, command, analysis),
    };
}

/**
 * Service script that analyzes the command it is given instead of running
 * it, and prints the analysis as JSON: the analyzer that recognized it and
 * what that analyzer found (with the outputs it inferred, the resolver's
 * diagnostics and the compilation database entries of the command), or why
 * none did.
 *
 * Output:
 * ```txt
 * {
 *   "analyzer": "compiler",
 *   "compiler": "builtin:clang",
 *   "dialect": "clang",
 *   "phase": "compile",
 *   "artifact": "object",
 *   ...
 * }
 * ```
 */
function inspect(): CatterContextService {
    const analyzers = new Registry<CommandAnalysis, CommandAnalyzerError>()
        .register(
            "compiler",
            new CompilerAnalyzer({ resolver: new CompilerResolver({ debug: true }) }),
        )
        .register("archiver", new ArchiverAnalyzer());

    return create({
        onStart(config) {
            config.execute = false;
            if (run(inspectCLI, config.scriptArgs) === undefined) {
                return config;
            }
            const argv = config.buildSystemCommand;
            if (argv.length === 0) {
                throw new Error("inspect: give the command to analyze after --");
            }

            const command = { exe: argv[0]!, argv };
            const analysis = analyzers.analyze(command);
            const described = analysis.isOk()
                ? describe(config.buildSystemCommandCwd, command, analysis.value)
                : {
                      analyzer: null,
                      errors: analysis.error.map((error) => ({
                          kind: error.kind,
                          message: error.message,
                      })),
                  };
            println(JSON.stringify(described, null, 2));
            return config;
        },
    });
}

register(inspect());
