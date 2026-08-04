import type { AnalyzedData } from "../model.js";
import type {
  CompilerDialect,
  CompilerIdentity,
  CompilerKind,
  CompilerMatcher,
  CompilerRule,
  CompilerTargetFact,
} from "./types.js";

import { platform } from "catter/os";

const VERSION_PATTERN = String.raw`(?:[-_]?\d+(?:[._-][0-9a-zA-Z]+)*)?`;
const COMPILER_PATTERN_FLAGS = platform() === "windows" ? "i" : "";

function createCompilerRegex(
  basePattern: string,
  withVersion: boolean,
): RegExp {
  const optionalPathPrefix = String.raw`(?:.*[\\/])?`;
  const versionPattern = withVersion ? VERSION_PATTERN : "";
  const executableSuffix = String.raw`(?:\.exe)?`;
  return new RegExp(
    `^${optionalPathPrefix}${basePattern}${versionPattern}${executableSuffix}$`,
    COMPILER_PATTERN_FLAGS,
  );
}

type BuiltinCompilerRule = {
  kind: CompilerKind;
  dialect: CompilerDialect;
  executable: RegExp;
  targetPrefix?: boolean;
};

const BUILTIN_COMPILER_RULES: readonly BuiltinCompilerRule[] = [
  {
    kind: "gcc",
    dialect: "gcc",
    executable: createCompilerRegex(
      String.raw`(?:([^\\/]+)-)?(?:cc|c\+\+)`,
      false,
    ),
    targetPrefix: true,
  },
  {
    kind: "gcc",
    dialect: "gcc",
    executable: createCompilerRegex(
      String.raw`(?:([^\\/]+)-)?(?:gcc|g\+\+)`,
      true,
    ),
    targetPrefix: true,
  },
  {
    kind: "clang",
    dialect: "clang",
    executable: createCompilerRegex(
      String.raw`(?:([^\\/]+)-)?clang(?:\+\+)?`,
      true,
    ),
    targetPrefix: true,
  },
  {
    kind: "clang-cl",
    dialect: "msvc",
    executable: createCompilerRegex(String.raw`(?:([^\\/]+)-)?clang-cl`, true),
    targetPrefix: true,
  },
  {
    kind: "msvc",
    dialect: "msvc",
    executable: createCompilerRegex(String.raw`cl`, false),
  },
  {
    kind: "nvcc",
    dialect: "nvcc",
    executable: createCompilerRegex(String.raw`(?:[^\\/]+-)?nvcc`, true),
  },
];

type BuiltinCompilerIdentity = {
  kind: CompilerKind;
  dialect: CompilerDialect;
  targetPrefix?: string;
};

function identifyBuiltinCompiler(executable: string): BuiltinCompilerIdentity {
  for (const rule of BUILTIN_COMPILER_RULES) {
    const match = rule.executable.exec(executable);
    if (match !== null) {
      return {
        kind: rule.kind,
        dialect: rule.dialect,
        targetPrefix: rule.targetPrefix === true ? match[1] : undefined,
      };
    }
  }

  return { kind: "unknown", dialect: "unknown" };
}

/** Identifies the builtin compiler family for an executable path or name. */
export function identifyCompiler(executable: string): CompilerKind {
  return identifyBuiltinCompiler(executable).kind;
}

function executableTargetFact(
  executable: string,
  prefix: string | undefined,
): CompilerTargetFact | undefined {
  if (prefix === undefined || prefix.length === 0) {
    return undefined;
  }

  return {
    target: { triple: prefix },
    source: {
      kind: "executable-prefix",
      executable,
      prefix,
    },
  };
}

export class CompilerIdentifier {
  private readonly customRules = new Map<string, CompilerRule>();

  /**
   * Registers or replaces a custom compiler identification rule.
   *
   * Custom rules are evaluated before builtin compiler detection. Use this for
   * cross compilers or project-specific driver names that should be parsed as one
   * of the builtin dialects.
   */
  registerCompilerRule(key: string, rule: CompilerRule): void {
    this.customRules.delete(key);
    this.customRules.set(key, rule);
  }

  /** Removes a previously registered custom compiler rule by key. */
  unregisterCompilerRule(key: string): void {
    this.customRules.delete(key);
  }

  /** Returns the currently registered custom compiler rules in match order. */
  compilerRules(): readonly CompilerRule[] {
    return [...this.customRules.values()];
  }

  /**
   * Identifies the compiler command and selects a builtin parser dialect, fall back to the `unknown` dialect
   */
  identifyCompilerCommand(command: AnalyzedData): CompilerIdentity {
    for (const [key, rule] of this.customRules) {
      if (!this.ruleMatches(rule, command)) {
        continue;
      }

      return {
        key,
        dialect: rule.dialect,
        target:
          rule.target === undefined
            ? undefined
            : {
                target: { ...rule.target },
                source: { kind: "compiler-rule", key },
              },
      };
    }

    const builtin = identifyBuiltinCompiler(command.exe);
    return {
      key: `builtin:${builtin.kind}`,
      dialect: builtin.dialect,
      target: executableTargetFact(command.exe, builtin.targetPrefix),
    };
  }

  private matcherMatches(
    matcher: CompilerMatcher,
    command: AnalyzedData,
  ): boolean {
    if (matcher instanceof RegExp) {
      matcher.lastIndex = 0;
      return matcher.test(command.exe);
    }

    return matcher(command);
  }

  private ruleMatches(rule: CompilerRule, command: AnalyzedData): boolean {
    const matchers = Array.isArray(rule.match) ? rule.match : [rule.match];
    return matchers.some((matcher) => this.matcherMatches(matcher, command));
  }
}
