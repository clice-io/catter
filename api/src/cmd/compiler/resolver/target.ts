import { platform } from "catter/os";
import { CompilerTargetResolutionError } from "../errors.js";
import {
  CompilerArtifactModel,
  CompilerObjectFormat,
  CompilerTargetEnv,
  CompilerTargetOS,
  type CompilerIdentity,
  type CompilerOutputConvention,
  type CompilerParseResult,
  type CompilerTarget,
  type CompilerTargetFact,
  type EffectiveCompilerTarget,
} from "../types.js";

export const ELF_OUTPUT_CONVENTION: CompilerOutputConvention = {
  object: ".o",
  executable: "",
  sharedLibrary: ".so",
  staticLibrary: ".a",
};

export const MACHO_OUTPUT_CONVENTION: CompilerOutputConvention = {
  object: ".o",
  executable: "",
  sharedLibrary: ".dylib",
  staticLibrary: ".a",
};

export const COFF_GNU_OUTPUT_CONVENTION: CompilerOutputConvention = {
  object: ".o",
  executable: ".exe",
  sharedLibrary: ".dll",
  staticLibrary: ".a",
};

export const COFF_MSVC_OUTPUT_CONVENTION: CompilerOutputConvention = {
  object: ".obj",
  executable: ".exe",
  sharedLibrary: ".dll",
  staticLibrary: ".lib",
};

function hasAny(tokens: readonly string[], values: readonly string[]): boolean {
  return values.some((value) => tokens.includes(value));
}

/** Classifies a target triple without applying host or driver fallbacks. */
export function targetFromTriple(triple: string): CompilerTarget {
  const lower = triple.toLowerCase();
  const tokens = lower.split(/[-_]/).filter((token) => token.length > 0);
  const target: CompilerTarget = { triple };

  if (
    hasAny(tokens, ["windows", "win32", "mingw32", "mingw64", "cygwin", "msys"])
  ) {
    target.os = CompilerTargetOS.Windows;
    target.objectFormat = CompilerObjectFormat.Coff;
  } else if (
    hasAny(tokens, ["darwin", "macos", "ios"]) ||
    tokens.includes("apple")
  ) {
    target.os = CompilerTargetOS.Darwin;
    target.objectFormat = CompilerObjectFormat.MachO;
  } else if (tokens.includes("linux")) {
    target.os = CompilerTargetOS.Linux;
    target.objectFormat = CompilerObjectFormat.Elf;
  }

  if (tokens.includes("msvc")) {
    target.env = CompilerTargetEnv.Msvc;
  } else if (tokens.some((token) => token.includes("mingw"))) {
    target.env = CompilerTargetEnv.Mingw;
  } else if (
    tokens.some(
      (token) =>
        token === "gnu" ||
        token.startsWith("gnu") ||
        token === "musl" ||
        token === "eabi" ||
        token === "eabihf",
    )
  ) {
    target.env = CompilerTargetEnv.Gnu;
  }

  target.artifactModel = artifactModelFromTarget(target);
  return target;
}

function clDriverTarget(): CompilerTargetFact {
  return {
    target: {
      triple: "unknown-pc-windows-msvc",
      os: CompilerTargetOS.Windows,
      env: CompilerTargetEnv.Msvc,
      objectFormat: CompilerObjectFormat.Coff,
      artifactModel: CompilerArtifactModel.CoffMsvc,
    },
    source: {
      kind: "driver-default",
      dialect: "msvc",
    },
  };
}

function hostTarget(): CompilerTargetFact {
  switch (platform()) {
    case "windows":
      return {
        target: {
          os: CompilerTargetOS.Windows,
          objectFormat: CompilerObjectFormat.Coff,
          artifactModel: CompilerArtifactModel.CoffGnu,
        },
        source: { kind: "host-fallback" },
      };
    case "macos":
      return {
        target: {
          os: CompilerTargetOS.Darwin,
          objectFormat: CompilerObjectFormat.MachO,
          artifactModel: CompilerArtifactModel.MachO,
        },
        source: { kind: "host-fallback" },
      };
    case "linux":
      return {
        target: {
          os: CompilerTargetOS.Linux,
          objectFormat: CompilerObjectFormat.Elf,
          artifactModel: CompilerArtifactModel.Elf,
        },
        source: { kind: "host-fallback" },
      };
  }
}

function artifactModelFromTarget(
  target: CompilerTarget,
): CompilerArtifactModel | undefined {
  if (target.artifactModel !== undefined) {
    return target.artifactModel;
  }

  if (
    target.objectFormat === CompilerObjectFormat.MachO ||
    target.os === CompilerTargetOS.Darwin
  ) {
    return CompilerArtifactModel.MachO;
  }

  if (
    target.objectFormat === CompilerObjectFormat.Elf ||
    target.os === CompilerTargetOS.Linux
  ) {
    return CompilerArtifactModel.Elf;
  }

  if (
    target.objectFormat === CompilerObjectFormat.Coff ||
    target.os === CompilerTargetOS.Windows
  ) {
    if (target.env === CompilerTargetEnv.Msvc) {
      return CompilerArtifactModel.CoffMsvc;
    }
    if (
      target.env === CompilerTargetEnv.Gnu ||
      target.env === CompilerTargetEnv.Mingw
    ) {
      return CompilerArtifactModel.CoffGnu;
    }
  }

  return undefined;
}

/** Returns the complete output convention for one resolver-ready artifact model. */
export function outputConventionFromArtifactModel(
  artifactModel: CompilerArtifactModel,
): CompilerOutputConvention {
  switch (artifactModel) {
    case CompilerArtifactModel.Elf:
      return ELF_OUTPUT_CONVENTION;
    case CompilerArtifactModel.MachO:
      return MACHO_OUTPUT_CONVENTION;
    case CompilerArtifactModel.CoffGnu:
      return COFF_GNU_OUTPUT_CONVENTION;
    case CompilerArtifactModel.CoffMsvc:
      return COFF_MSVC_OUTPUT_CONVENTION;
  }
}

/** Resolves target evidence into the minimum complete target needed by command resolution. */
export class CompilerTargetResolver {
  resolve(
    parsed: CompilerParseResult,
    identity: CompilerIdentity,
    override?: CompilerTarget,
  ): EffectiveCompilerTarget {
    const selected: CompilerTargetFact =
      override !== undefined
        ? {
            target: override,
            source: { kind: "resolver-override" },
          }
        : (parsed.target ??
          identity.target ??
          (parsed.dialect === "msvc" ? clDriverTarget() : hostTarget()));

    const descriptor =
      selected.target.triple === undefined
        ? selected.target
        : {
            ...targetFromTriple(selected.target.triple),
            ...selected.target,
          };
    const artifactModel = artifactModelFromTarget(descriptor);
    if (artifactModel === undefined) {
      const description = descriptor.triple ?? "structured target";
      throw new CompilerTargetResolutionError(
        `compiler target does not determine an artifact model: ${description}`,
      );
    }

    return {
      descriptor: { ...descriptor, artifactModel },
      artifactModel,
      source: selected.source,
    };
  }
}
