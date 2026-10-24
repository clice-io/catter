import {
    type CompilerInput,
    type CompilerInputRole,
    type CompilerInputSuffixRule,
    type CompilerParseResult,
} from "../types.js";
import type { CompilerResolverEffectiveOptions } from "./types.js";

import { ResolverTrace } from "./resolver.js";

export type ParsedRead = {
    readonly input: CompilerInput;
    readonly role: CompilerInputRole;
};

function isStreamPath(path: string): boolean {
    return path === "-";
}

function parserInputRole(input: CompilerInput): CompilerInputRole {
    if (input.source.kind === "remainder-argument" || input.source.kind === "remainder-option") {
        return "link";
    }

    return "source";
}

export function collectReads(
    parsed: CompilerParseResult,
    options: CompilerResolverEffectiveOptions,
    trace: ResolverTrace,
): ParsedRead[] {
    const parserReads = parsed.inputs.flatMap((input): ParsedRead[] => {
        if (isStreamPath(input.path)) {
            trace.ignoreStreamInput(input);
            return [];
        }

        return [
            {
                input,
                role: parserInputRole(input),
            },
        ];
    });

    const candidateReads = parsed.inputCandidates.flatMap((candidate) =>
        resolveInputCandidate(candidate, options.inputCandidates, trace),
    );

    return [...parserReads, ...candidateReads].sort(
        (left, right) => left.input.index - right.input.index,
    );
}

function resolveInputCandidate(
    candidate: CompilerInput,
    options: CompilerResolverEffectiveOptions["inputCandidates"],
    trace: ResolverTrace,
): ParsedRead[] {
    if (isStreamPath(candidate.path)) {
        trace.rejectInputCandidate(candidate, "stream input is not a filesystem dependency");
        return [];
    }

    let rules = options.withoutLanguage;

    const language = candidate.language?.toLowerCase();

    switch (language) {
        case "c":
        case "c++":
            rules = options.byLanguage[language];
        case "none":
        case undefined:
            break;
        default: {
            trace.rejectInputCandidate(
                candidate,
                "input candidate has unsupported explicit language",
            );
            return [];
        }
    }
    const lowerPath = candidate.path.toLowerCase();
    const suffixRule = rules.suffixRules.find((rule) =>
        normalizedSuffixes(rule).some((suffix) => lowerPath.endsWith(suffix)),
    );

    if (suffixRule !== undefined) {
        trace.acceptInputCandidate(candidate, suffixRule.role);
        return [
            {
                input: candidate,
                role: suffixRule.role,
            },
        ];
    }

    if (rules.unknownSuffix !== "reject") {
        trace.acceptInputCandidate(candidate, rules.unknownSuffix);
        return [
            {
                input: candidate,
                role: rules.unknownSuffix,
            },
        ];
    }

    trace.rejectInputCandidate(candidate, "input candidate did not match suffix rules");
    return [];
}

function normalizedSuffixes(rule: CompilerInputSuffixRule): readonly string[] {
    const suffixes = Array.isArray(rule.suffix) ? rule.suffix : [rule.suffix];
    return suffixes.map((suffix) => suffix.toLowerCase());
}
