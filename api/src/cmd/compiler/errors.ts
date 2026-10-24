import { AnalysisError } from "../model.js";

export class CompilerUnsupportedError extends AnalysisError {
    readonly kind = "compiler-unsupported" as const;
}

export class CompilerParseError extends AnalysisError {
    readonly kind = "compiler-parse" as const;
}

export class CompilerModelError extends AnalysisError {
    readonly kind = "compiler-model" as const;
}

export class CompilerResolverOptionsError extends AnalysisError {
    readonly kind = "compiler-resolver-options" as const;
}

export class CompilerTargetResolutionError extends AnalysisError {
    readonly kind = "compiler-target-resolution" as const;
}

export type CompilerAnalysisError =
    | CompilerUnsupportedError
    | CompilerParseError
    | CompilerModelError
    | CompilerResolverOptionsError
    | CompilerTargetResolutionError;

export function toCompilerAnalysisError(value: unknown, context: string): CompilerAnalysisError {
    if (
        value instanceof CompilerUnsupportedError ||
        value instanceof CompilerParseError ||
        value instanceof CompilerModelError ||
        value instanceof CompilerResolverOptionsError ||
        value instanceof CompilerTargetResolutionError
    ) {
        return value;
    }

    if (value instanceof Error) {
        return new CompilerParseError(`${context}: ${value.message}`);
    }

    return new CompilerParseError(`${context}: ${String(value)}`);
}
