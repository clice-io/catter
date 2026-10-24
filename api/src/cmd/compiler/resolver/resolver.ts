import {
    type CompilerInput,
    type CompilerInputCandidateDecision,
    type CompilerInputCandidateRules,
    type CompilerInputRole,
    type CompilerInputSuffixRule,
    type CompilerIdentity,
    type CompilerInferredWrite,
    type CompilerOutputConvention,
    type CompilerParseResult,
    type CompilerResolveDiagnostic,
    type CompilerResolveResult,
    type CompilerResolverOptions,
    type CompilerResolverSourceLanguage,
    type CompilerResolverWriteOptions,
} from "../types.js";
import { CompilerResolverOptionsError } from "../errors.js";
import { CompilerTargetResolver, outputConventionFromArtifactModel } from "./target.js";
import { collectReads } from "./reads.js";
import type {
    CompleteInputCandidateOptions,
    CompleteInputCandidateRules,
    CompilerResolverEffectiveOptions,
    NormalizedResolverOptions,
    PartialOutputConvention,
    ResolverInputCandidateOptions,
} from "./types.js";
import { resolveWrites } from "./writes.js";

/**
 * Resolver strategy:
 *
 * Parser results are treated as command-line facts, not as the final dependency
 * graph. The resolver first decides which uncertain path-like tokens are real
 * reads. Explicit language state only selects the suffix rule group for those
 * uncertain tokens; it does not by itself prove that a token is a source file.
 * Tokens already proven by parser syntax remain reads, with their role derived
 * from the kind of parser evidence rather than from suffix guessing.
 *
 * Read classification has two separate axes. Parser evidence says whether a
 * token is proven input syntax or only a candidate. Resolver role says which
 * part of the compiler pipeline consumes the accepted read. Frontend/source
 * reads participate in preprocessing and compilation, feed source-file reports,
 * and may name per-input compile outputs. Link-like reads participate in
 * linking, archiving, relocation, or device-link steps, but do not generate
 * per-source compile outputs. Suffix rules classify candidates into these
 * consumption roles; they are not a substitute for parser evidence.
 *
 * The intended precedence is semantic evidence first, suffix evidence last.
 * Parser facts such as linker remainder operands should directly determine the
 * read role. The resolved compiler mode can constrain how accepted reads are
 * used and which writes can exist, but a link-like mode flag such as shared
 * library output does not by itself prove that every positional token is a
 * source or link input. Suffix policy is for the remaining ambiguous positional
 * candidates after parser facts and mode semantics have been considered.
 *
 * Once reads are known, writes are inferred from the resolved compiler mode and
 * target output conventions. Target facts, not parser dialect, define platform
 * naming and link-input suffixes. Commands are assumed to be valid compiler
 * invocations; a token rejected by resolver policy means it is not visible as a
 * file dependency here, not that the original command is invalid.
 */

const C_SOURCE_SUFFIXES = [".c", ".i"] as const;

const CXX_SOURCE_SUFFIXES = [".c++", ".cc", ".cp", ".cpp", ".cxx", ".ii"] as const;

const DEFAULT_WRITE_OPTIONS: Required<CompilerResolverWriteOptions> = {
    inferDefaultOutputs: true,
    expandDirectoryOutputs: true,
    inferAssemblyListings: true,
};

const DEFAULT_C_CANDIDATE_RULES: CompleteInputCandidateRules = {
    suffixRules: suffixRulesFor(C_SOURCE_SUFFIXES, "source"),
    unknownSuffix: "reject",
};

const DEFAULT_CXX_CANDIDATE_RULES: CompleteInputCandidateRules = {
    suffixRules: suffixRulesFor(CXX_SOURCE_SUFFIXES, "source"),
    unknownSuffix: "reject",
};

function suffixRulesFor(
    suffixes: Iterable<string>,
    role: CompilerInputRole,
): CompilerInputSuffixRule[] {
    return Array.from(suffixes, (suffix) => ({ suffix, role }));
}

function completeInputCandidateRules(
    defaults: CompleteInputCandidateRules,
    override: CompilerInputCandidateRules | undefined,
): CompleteInputCandidateRules {
    return {
        suffixRules: override?.suffixRules ?? defaults.suffixRules,
        unknownSuffix: override?.unknownSuffix ?? defaults.unknownSuffix,
    };
}

function buildInputCandidateOptions(
    options: ResolverInputCandidateOptions,
    convention: CompilerOutputConvention,
): CompleteInputCandidateOptions {
    const byLanguage: Record<CompilerResolverSourceLanguage, CompleteInputCandidateRules> = {
        c: completeInputCandidateRules(DEFAULT_C_CANDIDATE_RULES, options?.byLanguage?.c),
        "c++": completeInputCandidateRules(
            DEFAULT_CXX_CANDIDATE_RULES,
            options?.byLanguage?.["c++"],
        ),
    };

    const defaultRulesWithoutLanguage: CompleteInputCandidateRules = {
        suffixRules: [
            ...byLanguage.c.suffixRules,
            ...byLanguage["c++"].suffixRules,
            ...suffixRulesFor(
                [convention.object, convention.sharedLibrary, convention.staticLibrary],
                "link",
            ),
        ],
        unknownSuffix: "reject",
    };

    return {
        byLanguage,
        withoutLanguage: completeInputCandidateRules(
            defaultRulesWithoutLanguage,
            options?.withoutLanguage,
        ),
    };
}

function buildEffectiveOptions(
    options: NormalizedResolverOptions,
    parsed: CompilerParseResult,
    identity: CompilerIdentity,
    targetResolver: CompilerTargetResolver,
): CompilerResolverEffectiveOptions {
    const target = targetResolver.resolve(parsed, identity, options.targetOverride);

    const convention = {
        ...outputConventionFromArtifactModel(target.artifactModel),
        ...options.outputConvention,
    };

    if (
        convention.object === undefined ||
        convention.executable === undefined ||
        convention.sharedLibrary === undefined ||
        convention.staticLibrary === undefined
    ) {
        throw new CompilerResolverOptionsError("compiler resolver output convention is incomplete");
    }
    return {
        target,
        outputConvention: convention as CompilerOutputConvention,
        inputCandidates: buildInputCandidateOptions(
            options.inputCandidates,
            convention as CompilerOutputConvention,
        ),
        writes: options.writes,
        debug: options.debug,
    };
}

export class ResolverTrace {
    readonly inputCandidateDecisions: CompilerInputCandidateDecision[] = [];
    readonly inferredWriteRecords: CompilerInferredWrite[] = [];
    readonly diagnosticRecords: CompilerResolveDiagnostic[] = [];

    acceptInputCandidate(input: CompilerInput, role: CompilerInputRole): void {
        this.inputCandidateDecisions.push({
            input,
            decision: "accepted",
            role,
        });
    }

    rejectInputCandidate(input: CompilerInput, reason: string): void {
        this.inputCandidateDecisions.push({
            input,
            decision: "rejected",
            reason,
        });
        this.addDiagnostic({
            code: "input-candidate-rejected",
            message: reason,
            path: input.path,
            index: input.index,
            source: input.source,
        });
    }

    ignoreStreamInput(input: CompilerInput): void {
        this.addDiagnostic({
            code: "stream-input-ignored",
            message: "stream input is not a filesystem dependency",
            path: input.path,
            index: input.index,
            source: input.source,
        });
    }

    addDiagnostic(diagnostic: CompilerResolveDiagnostic): void {
        this.diagnosticRecords.push(diagnostic);
    }

    inferredWrite(path: string, reason: CompilerInferredWrite["reason"]): void {
        this.inferredWriteRecords.push({ path, reason });
    }
}

/**
 * Resolves parsed compiler facts into visible file reads, writes, and dependency edges.
 */
export class CompilerResolver {
    private readonly options: NormalizedResolverOptions;
    private readonly targetResolver = new CompilerTargetResolver();

    constructor(options: CompilerResolverOptions = {}) {
        this.options = {
            targetOverride: options.targetOverride,
            outputConvention: options.outputConvention,
            inputCandidates: options.inputCandidates,
            writes: {
                ...DEFAULT_WRITE_OPTIONS,
                ...options.writes,
            },
            debug: options.debug ?? false,
        };
    }

    resolve(parsed: CompilerParseResult, identity: CompilerIdentity): CompilerResolveResult {
        const effectiveOptions = buildEffectiveOptions(
            this.options,
            parsed,
            identity,
            this.targetResolver,
        );
        const trace = new ResolverTrace();
        const reads = collectReads(parsed, effectiveOptions, trace);
        const writes = resolveWrites(parsed, reads, effectiveOptions, trace);

        const result: CompilerResolveResult = {
            target: effectiveOptions.target,
            reads: reads.map((read) => read.input.path),
            writes: writes.map((write) => write.path),
            edges: writes.map((write) => ({
                output: write.path,
                inputs: write.reads.map((read) => read.input.path),
            })),
            sourceFiles: reads
                .filter((read) => read.role === "source")
                .map((read) => read.input.path),
        };

        if (this.options.debug) {
            result.debug = {
                inputCandidates: trace.inputCandidateDecisions,
                inferredWrites: trace.inferredWriteRecords,
                diagnostics: trace.diagnosticRecords,
            };
        }

        return result;
    }
}
