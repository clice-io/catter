import { path as fsPath } from "catter/fs";
import {
  CompilerArtifact,
  CompilerDialect,
  CompilerPhase,
  type CompilerOutput,
  type CompilerOutputConvention,
  type CompilerOutputKind,
  type CompilerParseResult,
} from "../types.js";
import type { CompilerResolverEffectiveOptions } from "./types.js";
import { ResolverTrace } from "./resolver.js";
import { ParsedRead } from "./reads.js";

type ResolvedWrite = {
  readonly path: string;
  readonly reads: readonly ParsedRead[];
};

function pathStem(path: string): string {
  const name = fsPath.filename(path);
  const ext = fsPath.extension(name);

  return name.slice(0, name.length - ext.length);
}

function isDirectoryLike(path: string): boolean {
  return path.endsWith("/") || path.endsWith("\\");
}

export function resolveWrites(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): ResolvedWrite[] {
  return [
    ...resolvePrimaryWrites(parsed, reads, options, trace),
    ...resolveAssemblyListingWrites(parsed, reads, options, trace),
  ];
}

function resolvePrimaryWrites(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): ResolvedWrite[] {
  for (const output of parsed.outputCandidates) {
    trace.addDiagnostic({
      code: "output-candidate-ignored",
      message: "output candidates are not resolved in this resolver stage",
      path: output.path,
      index: output.index,
      source: output.source,
    });
  }

  if (parsed.compilerMode.artifact === CompilerArtifact.None) {
    return [];
  }

  const explicitOutput = selectExplicitOutput(parsed, trace);
  if (explicitOutput !== undefined) {
    return resolveExplicitPrimaryOutput(
      parsed,
      explicitOutput,
      reads,
      options,
      trace,
    );
  }

  if (!options.writes.inferDefaultOutputs) {
    return [];
  }

  return resolveDefaultPrimaryOutput(parsed, reads, options, trace);
}

function selectExplicitOutput(
  parsed: CompilerParseResult,
  trace: ResolverTrace,
): CompilerOutput | undefined {
  const acceptedKinds = phaseOutputKinds(parsed.compilerMode.phase);
  let selected: CompilerOutput | undefined;

  for (const output of [...parsed.outputs].sort(
    (left, right) => left.index - right.index,
  )) {
    if (acceptedKinds.includes(output.kind)) {
      selected = output;
      continue;
    }

    trace.addDiagnostic({
      code: "output-kind-ignored",
      message: `output kind ${output.kind} is not used for ${parsed.compilerMode.phase}`,
      path: output.path,
      index: output.index,
      source: output.source,
    });
  }

  return selected;
}

function phaseOutputKinds(phase: CompilerPhase): readonly CompilerOutputKind[] {
  switch (phase) {
    case CompilerPhase.Preprocess:
      return ["primary-artifact"];
    case CompilerPhase.SyntaxOnly:
      return [];
    case CompilerPhase.Compile:
      return ["primary-artifact", "object-file"];
    case CompilerPhase.Link:
      return ["primary-artifact", "linked-artifact"];
    case CompilerPhase.Archive:
    case CompilerPhase.RelocatableLink:
    case CompilerPhase.DeviceLink:
      return ["primary-artifact", "object-file", "linked-artifact"];
  }
}

function resolveExplicitPrimaryOutput(
  parsed: CompilerParseResult,
  output: CompilerOutput,
  reads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): ResolvedWrite[] {
  const relevantReads = primaryOutputReads(parsed, reads);
  const nameReads = primaryOutputNameReads(parsed, relevantReads);
  const paths = materializeOutputPaths(
    parsed.compilerMode.artifact,
    output.path,
    nameReads,
    options,
    trace,
  );
  return writesForPaths(paths, relevantReads);
}

function resolveDefaultPrimaryOutput(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): ResolvedWrite[] {
  switch (parsed.compilerMode.phase) {
    case CompilerPhase.Preprocess:
    case CompilerPhase.SyntaxOnly:
      return [];
    case CompilerPhase.Compile:
      return resolveDefaultCompileOutputs(parsed, reads, options, trace);
    case CompilerPhase.Link:
    case CompilerPhase.Archive:
    case CompilerPhase.RelocatableLink:
    case CompilerPhase.DeviceLink:
      return resolveDefaultSingleOutput(parsed, reads, options, trace);
  }
}

function resolveDefaultCompileOutputs(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): ResolvedWrite[] {
  const relevantReads = primaryOutputReads(parsed, reads);
  if (relevantReads.length === 0) {
    trace.addDiagnostic({
      code: "default-output-missing-input",
      message: "compile output has no source input to name",
    });
    return [];
  }
  const extension = artifactExtension(
    parsed.compilerMode.artifact,
    options.outputConvention,
  );
  if (extension === undefined) {
    trace.addDiagnostic({
      code: "default-output-unsupported-artifact",
      message: `cannot infer default output for ${parsed.compilerMode.artifact}`,
    });
    return [];
  }

  return relevantReads.map((read) => {
    const path = pathStem(read.input.path) + extension;
    trace.inferredWrite(path, "default-output");
    return {
      path,
      reads: [read],
    };
  });
}

function resolveDefaultSingleOutput(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): ResolvedWrite[] {
  const relevantReads = primaryOutputReads(parsed, reads);
  const firstRead = relevantReads[0];
  if (firstRead === undefined) {
    trace.addDiagnostic({
      code: "default-output-missing-input",
      message: "single output has no input to name",
    });
    return [];
  }

  const path = defaultSingleOutputPath(
    parsed,
    firstRead.input.path,
    options.outputConvention,
  );
  if (path === undefined) {
    trace.addDiagnostic({
      code: "implicit-output-unresolved",
      message: `cannot infer implicit output for ${parsed.dialect} ${parsed.compilerMode.phase}/${parsed.compilerMode.artifact}`,
    });
    return [];
  }

  trace.inferredWrite(path, "default-output");

  return [
    {
      path,
      reads: relevantReads,
    },
  ];
}

function artifactExtension(
  artifact: CompilerArtifact,
  convention: CompilerOutputConvention,
): string | undefined {
  switch (artifact) {
    case CompilerArtifact.Object:
      return convention.object;
    case CompilerArtifact.Executable:
      return convention.executable;
    case CompilerArtifact.SharedLibrary:
      return convention.sharedLibrary;
    case CompilerArtifact.StaticLibrary:
      return convention.staticLibrary;
    case CompilerArtifact.Assembly:
      return ".s";
    case CompilerArtifact.LlvmIR:
      return ".ll";
    case CompilerArtifact.LlvmBitcode:
      return ".bc";
    case CompilerArtifact.Pch:
      return ".pch";
    case CompilerArtifact.Pcm:
      return ".pcm";
    case CompilerArtifact.Ptx:
      return ".ptx";
    case CompilerArtifact.Cubin:
      return ".cubin";
    case CompilerArtifact.None:
    case CompilerArtifact.PreprocessedSource:
    case CompilerArtifact.Unknown:
      return undefined;
  }
}

function defaultSingleOutputPath(
  parsed: CompilerParseResult,
  inputPath: string,
  convention: CompilerOutputConvention,
): string | undefined {
  if (
    parsed.dialect !== CompilerDialect.Msvc ||
    parsed.compilerMode.phase !== CompilerPhase.Link
  ) {
    return undefined;
  }

  switch (parsed.compilerMode.artifact) {
    case CompilerArtifact.Executable:
      return pathStem(inputPath) + convention.executable;
    case CompilerArtifact.SharedLibrary:
      return pathStem(inputPath) + convention.sharedLibrary;
    default:
      return undefined;
  }
}

function primaryOutputReads(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
): readonly ParsedRead[] {
  switch (parsed.compilerMode.phase) {
    case CompilerPhase.Compile:
    case CompilerPhase.Preprocess:
      return reads.filter((read) => read.role === "source");
    case CompilerPhase.SyntaxOnly:
      return [];
    case CompilerPhase.Link:
    case CompilerPhase.Archive:
    case CompilerPhase.RelocatableLink:
    case CompilerPhase.DeviceLink:
      return reads;
  }
}

function primaryOutputNameReads(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
): readonly ParsedRead[] {
  if (parsed.compilerMode.phase === CompilerPhase.Compile) {
    return reads;
  }

  const firstRead = reads[0];
  return firstRead === undefined ? [] : [firstRead];
}

function materializeOutputPaths(
  artifact: CompilerArtifact,
  outputPath: string,
  nameReads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): string[] {
  if (
    !options.writes.expandDirectoryOutputs ||
    !isDirectoryLike(outputPath) ||
    nameReads.length === 0
  ) {
    return [outputPath];
  }

  const extension = artifactExtension(artifact, options.outputConvention);
  if (extension === undefined) {
    trace.addDiagnostic({
      code: "directory-output-unsupported-artifact",
      message: `cannot expand directory output for ${artifact}`,
      path: outputPath,
    });
    return [outputPath];
  }

  return nameReads.map((read) =>
    fsPath.lexicalNormal(
      fsPath.joinAll(outputPath, pathStem(read.input.path) + extension),
    ),
  );
}

function resolveAssemblyListingWrites(
  parsed: CompilerParseResult,
  reads: readonly ParsedRead[],
  options: CompilerResolverEffectiveOptions,
  trace: ResolverTrace,
): ResolvedWrite[] {
  if (!options.writes.inferAssemblyListings) {
    return [];
  }

  const listingPaths = parsed.compilerActions.flatMap((action) =>
    action.kind === "emit-assembly-listing" ? [action.path] : [],
  );

  if (listingPaths.length === 0 || !phaseCanEmitAssemblyListing(parsed)) {
    return [];
  }

  const sourceReads = reads.filter((read) => read.role === "source");
  for (const path of listingPaths.reverse()) {
    if (path !== undefined) {
      return resolveAssemblyListingWritesByPath(path, sourceReads, trace);
    }
  }

  return sourceReads.map((read) => {
    const path = pathStem(read.input.path) + ".asm";
    trace.inferredWrite(path, "assembly-listing");
    return {
      path,
      reads: [read],
    };
  });
}

function phaseCanEmitAssemblyListing(parsed: CompilerParseResult): boolean {
  switch (parsed.compilerMode.phase) {
    case CompilerPhase.Preprocess:
    case CompilerPhase.SyntaxOnly:
      return false;
    case CompilerPhase.Compile:
    case CompilerPhase.Link:
    case CompilerPhase.Archive:
    case CompilerPhase.RelocatableLink:
    case CompilerPhase.DeviceLink:
      return true;
  }
}

function resolveAssemblyListingWritesByPath(
  explicitPath: string,
  sourceReads: readonly ParsedRead[],
  trace: ResolverTrace,
): ResolvedWrite[] {
  if (isDirectoryLike(explicitPath)) {
    return sourceReads.map((read) => {
      const path = fsPath.lexicalNormal(
        fsPath.joinAll(explicitPath, pathStem(read.input.path) + ".asm"),
      );
      trace.inferredWrite(path, "assembly-listing");
      return {
        path,
        reads: [read],
      };
    });
  }

  if (sourceReads.length === 1) {
    return [explicitWrite(explicitPath, sourceReads)];
  }

  trace.addDiagnostic({
    code: "assembly-listing-ambiguous-output",
    message: "single assembly listing path cannot name multiple source inputs",
    path: explicitPath,
  });

  return [];
}

function writesForPaths(
  paths: readonly string[],
  reads: readonly ParsedRead[],
): ResolvedWrite[] {
  if (paths.length === reads.length) {
    return paths.map((path, index) => explicitWrite(path, [reads[index]!]));
  }
  return paths.map((path) => explicitWrite(path, reads));
}

function explicitWrite(
  path: string,
  reads: readonly ParsedRead[],
): ResolvedWrite {
  return {
    path,
    reads,
  };
}
