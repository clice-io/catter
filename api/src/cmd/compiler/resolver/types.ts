import type {
    CompilerInputCandidateOptions,
    CompilerInputCandidateRules,
    CompilerOutputConvention,
    CompilerResolverOptions,
    CompilerResolverSourceLanguage,
    CompilerResolverWriteOptions,
    EffectiveCompilerTarget,
} from "../types.js";

export type CompilerResolverEffectiveOptions = {
    target: EffectiveCompilerTarget;
    outputConvention: CompilerOutputConvention;
    inputCandidates: {
        byLanguage: Record<CompilerResolverSourceLanguage, Required<CompilerInputCandidateRules>>;
        withoutLanguage: Required<CompilerInputCandidateRules>;
    };
    writes: Required<CompilerResolverWriteOptions>;
    debug: boolean;
};

export type NormalizedResolverOptions = Omit<CompilerResolverOptions, "debug" | "writes"> & {
    readonly debug: boolean;
    readonly writes: Required<CompilerResolverWriteOptions>;
};

export type CompleteInputCandidateRules = Required<CompilerInputCandidateRules>;

export type CompleteInputCandidateOptions = CompilerResolverEffectiveOptions["inputCandidates"];

export type PartialOutputConvention = Partial<CompilerOutputConvention> | undefined;

export type ResolverInputCandidateOptions = CompilerInputCandidateOptions | undefined;
