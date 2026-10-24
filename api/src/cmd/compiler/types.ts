import type { AnalyzedData, Edge } from "../model.js";
import type { CompilerIdentifier } from "./identify.js";

/** Compiler executable families recognized by the builtin identifier. */
export const CompilerKind = ["gcc", "clang", "clang-cl", "msvc", "nvcc", "unknown"] as const;

/** Union of compiler executable families recognized by the builtin identifier. */
export type CompilerKind = (typeof CompilerKind)[number];

/** High-level compiler pipeline phase. */
export const CompilerPhase = {
    Preprocess: "preprocess",
    SyntaxOnly: "syntax-only",
    Compile: "compile",
    Link: "link",
    Archive: "archive",
    RelocatableLink: "relocatable-link",
    DeviceLink: "device-link",
} as const;

/** Union of high-level compiler pipeline phase values. */
export type CompilerPhase = (typeof CompilerPhase)[keyof typeof CompilerPhase];

/** Main artifact kind produced by a compiler command. */
export const CompilerArtifact = {
    None: "none",
    PreprocessedSource: "preprocessed-source",
    Object: "object",
    Executable: "exe",
    SharedLibrary: "shared",
    StaticLibrary: "static-lib",
    Assembly: "asm",
    LlvmIR: "llvm-ir",
    LlvmBitcode: "llvm-bc",
    Pch: "pch",
    Pcm: "pcm",
    Ptx: "ptx",
    Cubin: "cubin",
    Fatbin: "fatbin",
    Unknown: "unknown",
} as const;

/** Union of main artifact kind values produced by compiler commands. */
export type CompilerArtifact = (typeof CompilerArtifact)[keyof typeof CompilerArtifact];

/** Compiler phase and produced artifact content kind inferred from driver options. */
export type CompilerMode =
    | {
          phase: typeof CompilerPhase.Preprocess;
          artifact: typeof CompilerArtifact.PreprocessedSource;
      }
    | {
          phase: typeof CompilerPhase.SyntaxOnly;
          artifact: typeof CompilerArtifact.None;
      }
    | {
          phase: typeof CompilerPhase.Compile;
          artifact:
              | typeof CompilerArtifact.Object
              | typeof CompilerArtifact.Assembly
              | typeof CompilerArtifact.LlvmIR
              | typeof CompilerArtifact.LlvmBitcode
              | typeof CompilerArtifact.Pch
              | typeof CompilerArtifact.Pcm
              | typeof CompilerArtifact.Ptx
              | typeof CompilerArtifact.Cubin
              | typeof CompilerArtifact.Fatbin
              | typeof CompilerArtifact.Unknown;
      }
    | {
          phase: typeof CompilerPhase.Link;
          artifact: typeof CompilerArtifact.Executable | typeof CompilerArtifact.SharedLibrary;
      }
    | {
          phase: typeof CompilerPhase.Archive;
          artifact: typeof CompilerArtifact.StaticLibrary;
      }
    | {
          phase: typeof CompilerPhase.RelocatableLink | typeof CompilerPhase.DeviceLink;
          artifact: typeof CompilerArtifact.Object;
      };

/** Built-in parser dialect selected after command identification. */
export const CompilerDialect = {
    Clang: "clang",
    Gcc: "gcc",
    Msvc: "msvc",
    Nvcc: "nvcc",
    Unknown: "unknown",
} as const;

/** Union of parser dialect values selected during compiler identification. */
export type CompilerDialect = (typeof CompilerDialect)[keyof typeof CompilerDialect];

/** Coarse target operating system used for output naming conventions. */
export const CompilerTargetOS = {
    Linux: "linux",
    Darwin: "darwin",
    Windows: "windows",
    Unknown: "unknown",
} as const;

/** Union of target operating system identifiers. */
export type CompilerTargetOS = (typeof CompilerTargetOS)[keyof typeof CompilerTargetOS];

/** Coarse target environment or ABI family used for output naming conventions. */
export const CompilerTargetEnv = {
    Gnu: "gnu",
    Mingw: "mingw",
    Msvc: "msvc",
    Unknown: "unknown",
} as const;

/** Union of target environment identifiers. */
export type CompilerTargetEnv = (typeof CompilerTargetEnv)[keyof typeof CompilerTargetEnv];

/** Coarse object format used for output naming conventions. */
export const CompilerObjectFormat = {
    Elf: "elf",
    MachO: "macho",
    Coff: "coff",
    Unknown: "unknown",
} as const;

/** Union of object format identifiers. */
export type CompilerObjectFormat = (typeof CompilerObjectFormat)[keyof typeof CompilerObjectFormat];

/** Resolver-ready artifact and ABI convention selected for a compiler target. */
export const CompilerArtifactModel = {
    Elf: "elf",
    MachO: "macho",
    CoffGnu: "coff-gnu",
    CoffMsvc: "coff-msvc",
} as const;

/** Union of resolver-ready compiler artifact models. */
export type CompilerArtifactModel =
    (typeof CompilerArtifactModel)[keyof typeof CompilerArtifactModel];

/** Raw or partially classified target facts from one target source. */
export type CompilerTarget = {
    triple?: string;
    os?: CompilerTargetOS;
    env?: CompilerTargetEnv;
    objectFormat?: CompilerObjectFormat;
    /** Explicit artifact model when a triple is unavailable or custom. */
    artifactModel?: CompilerArtifactModel;
};

/** Evidence source for a compiler target fact. */
export type CompilerTargetSource =
    | {
          kind: "argument";
          option: string;
          index: number;
      }
    | {
          kind: "compiler-rule";
          key: string;
      }
    | {
          kind: "executable-prefix";
          executable: string;
          prefix: string;
      }
    | {
          kind: "driver-default";
          dialect: CompilerDialect;
      }
    | {
          kind: "resolver-override";
      }
    | {
          kind: "host-fallback";
      };

/** One target fact together with the evidence that produced it. */
export type CompilerTargetFact = {
    target: CompilerTarget;
    source: CompilerTargetSource;
};

/** Fully classified target accepted by command resolution. */
export type EffectiveCompilerTarget = {
    /** Winning target descriptor. Lower-priority descriptors are never merged into it. */
    descriptor: CompilerTarget;
    /** Mandatory minimum target information used for file conventions. */
    artifactModel: CompilerArtifactModel;
    /** Evidence that selected the winning descriptor. */
    source: CompilerTargetSource;
};

/** Output suffix convention used by the resolver when default outputs are inferred. */
export type CompilerOutputConvention = {
    object: string;
    executable: string;
    sharedLibrary: string;
    staticLibrary: string;
};

export type CompilerFactSource =
    | {
          /** Ordinary command-line argument. Usually ambiguous until the resolver applies policy. */
          kind: "argument";
      }
    | {
          /** Value attached to a recognized option. The option spelling provides parser semantics. */
          kind: "option";
          option: string;
          optionIndex: number;
      }
    | {
          /** Argument captured after a parser-recognized remainder boundary, such as a linker tail. */
          kind: "remainder-argument";
          boundary: string;
          boundaryIndex: number;
      }
    | {
          /** Option captured after a parser-recognized remainder boundary, such as a linker tail option. */
          kind: "remainder-option";
          boundary: string;
          boundaryIndex: number;
          option: string;
          optionIndex: number;
      };

/** One parsed compiler action before mode resolution. */
export type CompilerAction =
    | {
          /** Driver action represented by the parsed option. */
          kind:
              | "preprocess"
              | "syntax-only"
              | "compile-object"
              | "compile-assembly-like"
              | "compile-llvm-like"
              | "compile-pch"
              | "compile-pcm"
              | "unknown-compile-action"
              | "link-shared-library"
              | "archive"
              | "relocatable-link";
          /** Original argv index for the action option token. */
          index: number;
      }
    | {
          /** Side assembly/listing output requested by a CL-style option. */
          kind: "emit-assembly-listing";
          /** Original argv index for the action option token. */
          index: number;
          /** Optional /Fa path token; omitted when the driver uses per-input defaults. */
          path?: string;
      };

export type CompilerInput = {
    /** Path token as it appeared in the compiler command. */
    path: string;
    /** Original argv index for the input token. */
    index: number;
    /** Parser evidence for this input fact. */
    source: CompilerFactSource;
    /** Explicit language state in effect when this token was parsed; this is not proof that the token is a source input. */
    language?: string;
};

export type CompilerOutputKind = "primary-artifact" | "object-file" | "linked-artifact";

export type CompilerOutput = {
    /** Path token as it appeared in the compiler command. */
    path: string;
    /** Driver-level output purpose selected by the option spelling. */
    kind: CompilerOutputKind;
    /** Original argv index for the output option token. */
    index: number;
    /** Parser evidence for this output fact. */
    source: CompilerFactSource;
};

export type CompilerParseResult = {
    /** Parser dialect selected for this command line. This describes parser syntax only, not the target platform. */
    dialect: CompilerDialect;
    /** Final explicit target fact parsed from command-line arguments, when present. */
    target?: CompilerTargetFact;
    /** High-level action and primary artifact selected from parsed driver options. */
    compilerMode: CompilerMode;
    /** Semantic action facts collected from options before mode resolution. */
    compilerActions: CompilerAction[];
    /** Path-like tokens that may be real inputs, but still require resolver policy to classify or reject. */
    inputCandidates: CompilerInput[];
    /** Output-like tokens that were parsed but are not yet accepted for the resolved compiler mode. */
    outputCandidates: CompilerOutput[];
    /** Inputs proven by parser syntax, such as option-bound source operands or linker remainder operands. */
    inputs: CompilerInput[];
    /** Outputs proven by parser syntax, such as explicit primary/object/linked output options. */
    outputs: CompilerOutput[];
};

/**
 * Resolver-side read consumption role.
 *
 * This is not parser evidence and does not describe where the token came from.
 * `source` means a read belongs to the compiler frontend/preprocess/compile
 * side of the command, while `link` means it is consumed by link-like phases
 * and should not produce per-source compile outputs.
 */
export type CompilerInputRole = "source" | "link";

/** Source languages currently understood by resolver suffix policies. */
export type CompilerResolverSourceLanguage = "c" | "c++";

/** One suffix-to-consumption-role rule used after parser and mode semantics do not fully classify a candidate. */
export type CompilerInputSuffixRule = {
    /** File suffix or suffixes, matched case-insensitively. */
    suffix: string | readonly string[];
    /** Read consumption role assigned when the suffix matches. */
    role: CompilerInputRole;
};

/** Rules used for one input suffix group while resolving input candidates. */
export type CompilerInputCandidateRules = {
    /** Suffix rules. When omitted, the resolver uses its built-in rules for this group. */
    suffixRules?: readonly CompilerInputSuffixRule[];
    /** Role for unknown suffixes, or reject. */
    unknownSuffix?: "reject" | CompilerInputRole;
};

/** Input-candidate resolver configuration split by parser language state. */
export type CompilerInputCandidateOptions = {
    /** Rules used when the parser candidate has an explicit supported source language. */
    byLanguage?: Partial<Record<CompilerResolverSourceLanguage, CompilerInputCandidateRules>>;
    /** Rules used when the parser candidate has no language or language `none`. */
    withoutLanguage?: CompilerInputCandidateRules;
};

/** Output resolver configuration for inferred writes and side effects. */
export type CompilerResolverWriteOptions = {
    /** Whether to infer driver default outputs when no matching explicit output fact exists. */
    inferDefaultOutputs?: boolean;
    /** Whether directory-like explicit output paths are expanded into per-input file paths. */
    expandDirectoryOutputs?: boolean;
    /** Whether CL-style assembly listing actions should add side-effect writes. */
    inferAssemblyListings?: boolean;
};

/** Configures how compiler parser facts are resolved into visible file reads, writes, and edges. */
export type CompilerResolverOptions = {
    /** Forced target override used before command and executable target facts. */
    targetOverride?: CompilerTarget;
    /** Explicit output suffix convention used before target-derived conventions. */
    outputConvention?: Partial<CompilerOutputConvention>;
    /** Rules for promoting parser input candidates into reads. */
    inputCandidates?: CompilerInputCandidateOptions;
    /** Rules for resolving writes from explicit outputs, compiler mode, and reads. */
    writes?: CompilerResolverWriteOptions;
    /** Whether to attach detailed resolver decisions and diagnostics to the result. */
    debug?: boolean;
};

/** Machine-readable resolver diagnostic used for debugging parser or resolver coverage gaps. */
export type CompilerResolveDiagnostic = {
    /** Stable diagnostic identifier. */
    code: string;
    /** Human-readable diagnostic details. */
    message: string;
    /** Related path token, when the diagnostic is tied to a specific file-like value. */
    path?: string;
    /** Related argv index, when available from the parser fact. */
    index?: number;
    /** Parser evidence associated with the diagnostic, when available. */
    source?: CompilerFactSource;
};

/** Resolver decision for one parser input candidate. */
export type CompilerInputCandidateDecision =
    | {
          /** Candidate fact supplied by the parser. */
          input: CompilerInput;
          /** Candidate was promoted to a read. */
          decision: "accepted";
          /** Read consumption role assigned by resolver policy. */
          role: CompilerInputRole;
      }
    | {
          /** Candidate fact supplied by the parser. */
          input: CompilerInput;
          /** Candidate was not visible as a dependency. */
          decision: "rejected";
          /** Short policy reason for rejection. */
          reason: string;
      };

/** Inferred write recorded for resolver debugging. */
export type CompilerInferredWrite = {
    /** Output path inferred by the resolver. */
    path: string;
    /** Why this write was inferred. */
    reason: "default-output" | "assembly-listing";
};

/** Optional detailed trace of resolver decisions. */
export type CompilerResolveDebug = {
    /** Resolver decisions for ambiguous parser input candidates, in argv order. */
    inputCandidates: CompilerInputCandidateDecision[];
    /** Writes inferred from compiler defaults or side-output conventions. */
    inferredWrites: CompilerInferredWrite[];
    /** Diagnostics emitted while resolving parser facts. */
    diagnostics: CompilerResolveDiagnostic[];
};

export type CompilerResolveResult = {
    target: EffectiveCompilerTarget;
    reads: string[];
    writes: string[];
    edges: Edge[];
    sourceFiles: string[];
    debug?: CompilerResolveDebug;
};

/** Resolver protocol accepted by the compiler analyzer. */
export interface CompilerResolverLike {
    resolve(parsed: CompilerParseResult, identity: CompilerIdentity): CompilerResolveResult;
}

export type CompilerAnalyzerOptions = {
    identifier?: CompilerIdentifier;
    resolver?: CompilerResolverLike;
};

/**
 * Matcher used by a custom compiler rule.
 *
 * Regular expressions are tested against the captured executable path or name.
 * Function matchers receive the command invocation and can inspect `exe` and
 * `argv`.
 */
export type CompilerMatcher = RegExp | ((command: AnalyzedData) => boolean);

/** Custom rule that maps an executable pattern to one builtin parser dialect. */
export interface CompilerRule {
    /** Builtin parser dialect to use after this rule matches. */
    dialect: CompilerDialect;
    /** One matcher or a list of alternative matchers. */
    match: CompilerMatcher | CompilerMatcher[];
    /** Target facts for renamed or project-specific compiler executables. */
    target?: CompilerTarget;
}

/** Result of identifying a command before builtin parser dispatch. */
export interface CompilerIdentity {
    /** Builtin identity key or custom rule key. */
    key: string;
    /** Parser dialect selected for the command, or `unknown` when unsupported. */
    dialect: CompilerDialect;
    /** Target fact inferred from the executable name or supplied by a custom rule. */
    target?: CompilerTargetFact;
}

/** Result of the unwrap stage before compiler identification. */
export interface UnwrappedCompilerCommand {
    /** Executable path or name after wrapper removal. Currently this is unchanged. */
    exe: string;
    /** Command argv after wrapper removal. Currently this is unchanged. */
    argv: readonly string[];
}
