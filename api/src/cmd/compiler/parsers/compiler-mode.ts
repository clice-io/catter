import {
    CompilerArtifact,
    CompilerDialect,
    CompilerPhase,
    type CompilerAction,
    type CompilerMode,
} from "../types.js";

type CompileArtifact = Extract<CompilerMode, { phase: typeof CompilerPhase.Compile }>["artifact"];

type LinkPlan = "executable" | "shared-library" | "static-library" | "relocatable-object";

type CompilerPipelineFacts = {
    preprocess: boolean;
    syntaxOnly: boolean;
    compileArtifact: CompileArtifact | undefined;
    sawAssemblyLike: boolean;
    sawLlvmLike: boolean;
    linkPlan: LinkPlan;
};

function collectInitialPipelineFacts(): CompilerPipelineFacts {
    return {
        preprocess: false,
        syntaxOnly: false,
        compileArtifact: undefined,
        sawAssemblyLike: false,
        sawLlvmLike: false,
        linkPlan: "executable",
    };
}

function collectGnuPipelineFacts(actions: readonly CompilerAction[]): CompilerPipelineFacts {
    const facts = collectInitialPipelineFacts();

    for (const action of actions) {
        switch (action.kind) {
            case "preprocess":
                facts.preprocess = true;
                break;
            case "syntax-only":
                facts.syntaxOnly = true;
                break;
            case "compile-object":
                if (facts.compileArtifact === undefined) {
                    facts.compileArtifact = CompilerArtifact.Object;
                    break;
                }
                if (facts.compileArtifact === CompilerArtifact.Unknown) {
                    facts.compileArtifact = CompilerArtifact.Object;
                }
                break;
            case "compile-assembly-like":
                facts.sawAssemblyLike = true;
                if (facts.sawLlvmLike) {
                    facts.compileArtifact = CompilerArtifact.LlvmIR;
                    break;
                }
                facts.compileArtifact = CompilerArtifact.Assembly;
                break;
            case "compile-llvm-like":
                facts.sawLlvmLike = true;
                if (facts.sawAssemblyLike) {
                    facts.compileArtifact = CompilerArtifact.LlvmIR;
                    break;
                }
                facts.compileArtifact = CompilerArtifact.LlvmBitcode;
                break;
            case "compile-pch":
                facts.compileArtifact = CompilerArtifact.Pch;
                break;
            case "compile-pcm":
                facts.compileArtifact = CompilerArtifact.Pcm;
                break;
            case "unknown-compile-action":
                if (facts.compileArtifact === undefined) {
                    facts.compileArtifact = CompilerArtifact.Unknown;
                }
                break;
            case "link-shared-library":
                if (facts.linkPlan === "executable") {
                    facts.linkPlan = "shared-library";
                }
                break;
            case "archive":
                if (facts.linkPlan === "executable") {
                    facts.linkPlan = "static-library";
                    break;
                }
                if (facts.linkPlan === "shared-library") {
                    facts.linkPlan = "static-library";
                }
                break;
            case "relocatable-link":
                if (facts.linkPlan === "executable") {
                    facts.linkPlan = "relocatable-object";
                    break;
                }
                if (facts.linkPlan === "shared-library") {
                    facts.linkPlan = "relocatable-object";
                }
                break;
            case "emit-assembly-listing":
                break;
        }
    }

    return facts;
}

function collectClangClPipelineFacts(actions: readonly CompilerAction[]): CompilerPipelineFacts {
    const facts = collectInitialPipelineFacts();

    for (const action of actions) {
        switch (action.kind) {
            case "preprocess":
                facts.preprocess = true;
                break;
            case "syntax-only":
                facts.syntaxOnly = true;
                break;
            case "compile-object":
                if (facts.compileArtifact === undefined) {
                    facts.compileArtifact = CompilerArtifact.Object;
                    break;
                }
                if (facts.compileArtifact === CompilerArtifact.Unknown) {
                    facts.compileArtifact = CompilerArtifact.Object;
                }
                break;
            case "compile-assembly-like":
            case "compile-llvm-like":
            case "compile-pch":
            case "compile-pcm":
                break;
            case "unknown-compile-action":
                if (facts.compileArtifact === undefined) {
                    facts.compileArtifact = CompilerArtifact.Unknown;
                }
                break;
            case "link-shared-library":
                if (facts.linkPlan === "executable") {
                    facts.linkPlan = "shared-library";
                }
                break;
            case "archive":
                if (facts.linkPlan === "executable") {
                    facts.linkPlan = "static-library";
                    break;
                }
                if (facts.linkPlan === "shared-library") {
                    facts.linkPlan = "static-library";
                }
                break;
            case "relocatable-link":
                if (facts.linkPlan === "executable") {
                    facts.linkPlan = "relocatable-object";
                    break;
                }
                if (facts.linkPlan === "shared-library") {
                    facts.linkPlan = "relocatable-object";
                }
                break;
            case "emit-assembly-listing":
                break;
        }
    }

    return facts;
}

function resolvePipelineFacts(facts: CompilerPipelineFacts): CompilerMode {
    if (facts.preprocess) {
        return {
            phase: CompilerPhase.Preprocess,
            artifact: CompilerArtifact.PreprocessedSource,
        };
    }

    if (facts.syntaxOnly) {
        return {
            phase: CompilerPhase.SyntaxOnly,
            artifact: CompilerArtifact.None,
        };
    }

    if (facts.compileArtifact !== undefined) {
        return {
            phase: CompilerPhase.Compile,
            artifact: facts.compileArtifact,
        };
    }

    switch (facts.linkPlan) {
        case "executable":
            return {
                phase: CompilerPhase.Link,
                artifact: CompilerArtifact.Executable,
            };
        case "shared-library":
            return {
                phase: CompilerPhase.Link,
                artifact: CompilerArtifact.SharedLibrary,
            };
        case "static-library":
            return {
                phase: CompilerPhase.Archive,
                artifact: CompilerArtifact.StaticLibrary,
            };
        case "relocatable-object":
            return {
                phase: CompilerPhase.RelocatableLink,
                artifact: CompilerArtifact.Object,
            };
    }
}

export function resolveCompilerMode(
    dialect: CompilerDialect,
    actions: readonly CompilerAction[],
): CompilerMode {
    switch (dialect) {
        case CompilerDialect.Msvc:
            return resolvePipelineFacts(collectClangClPipelineFacts(actions));
        case CompilerDialect.Clang:
        case CompilerDialect.Gcc:
        case CompilerDialect.Nvcc:
        case CompilerDialect.Unknown:
            return resolvePipelineFacts(collectGnuPipelineFacts(actions));
    }
}
