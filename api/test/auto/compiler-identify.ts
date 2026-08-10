import {
  CompilerDialect,
  CompilerIdentifier,
  identifyCompiler,
  type CompilerKind,
} from "catter/cmd";
import { platform } from "catter/os";

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

const compilerCases: readonly {
  executable: string;
  expected: CompilerKind;
}[] = [
  { executable: "gcc", expected: "gcc" },
  { executable: "g++", expected: "gcc" },
  { executable: "gcc-10", expected: "gcc" },
  { executable: "g++-10.2", expected: "gcc" },
  { executable: "/usr/bin/gcc", expected: "gcc" },
  { executable: "/usr/bin/g++", expected: "gcc" },
  {
    executable: "/usr/bin/x86_64-linux-gnu-gcc-13",
    expected: "gcc",
  },
  { executable: "/usr/bin/aarch64-linux-gnu-c++", expected: "gcc" },
  { executable: "/usr/local/gcc-15.1.0/bin/c++", expected: "gcc" },
  {
    executable:
      "/usr/local/gcc-15.1.0/libexec/gcc/x86_64-pc-linux-gnu/15.1.0/cc1plus",
    expected: "unknown",
  },
  { executable: "cc1", expected: "unknown" },
  { executable: "f951", expected: "unknown" },
  { executable: "collect2", expected: "unknown" },
  { executable: "lto1", expected: "unknown" },
  {
    executable: String.raw`C:\msys64\ucrt64\bin\gcc.exe`,
    expected: "gcc",
  },
  {
    executable: String.raw`C:\msys64\ucrt64\bin\g++.exe`,
    expected: "gcc",
  },
  {
    executable: "C:/msys64/ucrt64/bin/x86_64-w64-mingw32-g++.exe",
    expected: "gcc",
  },
  { executable: "clang", expected: "clang" },
  { executable: "clang++", expected: "clang" },
  { executable: "clang-12", expected: "clang" },
  { executable: "clang++-20", expected: "clang" },
  { executable: "/usr/bin/clang", expected: "clang" },
  { executable: "/usr/bin/clang++", expected: "clang" },
  { executable: "/opt/homebrew/opt/llvm/bin/clang++", expected: "clang" },
  {
    executable: "/opt/llvm-20/bin/aarch64-apple-darwin23-clang++",
    expected: "clang",
  },
  { executable: String.raw`D:\LLVM\bin\clang.exe`, expected: "clang" },
  { executable: String.raw`D:\LLVM\bin\clang++.exe`, expected: "clang" },
  {
    executable: "C:/Program Files/LLVM/bin/clang.exe",
    expected: "clang",
  },
  { executable: "clang-cl", expected: "clang-cl" },
  { executable: "clang-cl.exe", expected: "clang-cl" },
  { executable: "clang-cl-18", expected: "clang-cl" },
  { executable: "clang-cl_20.1", expected: "clang-cl" },
  {
    executable: String.raw`C:\Program Files\LLVM\bin\clang-cl.exe`,
    expected: "clang-cl",
  },
  {
    executable: String.raw`D:\LLVM\bin\clang-cl.exe`,
    expected: "clang-cl",
  },
  {
    executable: "C:/Program Files/LLVM/bin/clang-cl.exe",
    expected: "clang-cl",
  },
  {
    executable: "x86_64-pc-windows-msvc-clang-cl.exe",
    expected: "clang-cl",
  },
  { executable: "cl", expected: "msvc" },
  { executable: "cl.exe", expected: "msvc" },
  {
    executable: String.raw`C:\Program Files\Microsoft Visual Studio\VC\Tools\MSVC\bin\cl.exe`,
    expected: "msvc",
  },
  {
    executable: String.raw`D:\MSVC\BuildTools\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe`,
    expected: "msvc",
  },
  {
    executable: String.raw`C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`,
    expected: "msvc",
  },
  {
    executable:
      "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe",
    expected: "msvc",
  },
  {
    executable: String.raw`C:\PROGRA~1\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`,
    expected: "msvc",
  },
  { executable: "gfortran", expected: "unknown" },
  { executable: "egfortran", expected: "unknown" },
  { executable: "f95", expected: "unknown" },
  { executable: "aarch64-linux-gnu-gfortran-15", expected: "unknown" },
  { executable: "flang", expected: "unknown" },
  { executable: "flang-new", expected: "unknown" },
  { executable: "/opt/llvm/bin/flang-new", expected: "unknown" },
  { executable: "aarch64-linux-gnu-flang-19", expected: "unknown" },
  { executable: "ifort", expected: "unknown" },
  { executable: "ifx", expected: "unknown" },
  {
    executable: String.raw`C:\Program Files (x86)\Intel\oneAPI\compiler\latest\bin\ifx.exe`,
    expected: "unknown",
  },
  { executable: "crayftn", expected: "unknown" },
  { executable: "ftn", expected: "unknown" },
  { executable: "/opt/cray/pe/craype/default/bin/ftn", expected: "unknown" },
  { executable: "nvcc", expected: "nvcc" },
  { executable: "nvcc-12.6", expected: "nvcc" },
  { executable: "/usr/local/cuda/bin/nvcc", expected: "nvcc" },
  {
    executable: String.raw`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\nvcc.exe`,
    expected: "nvcc",
  },
  { executable: "ccache", expected: "unknown" },
  { executable: "distcc", expected: "unknown" },
  { executable: "sccache", expected: "unknown" },
  { executable: "/usr/lib/ccache/ccache", expected: "unknown" },
  {
    executable: String.raw`C:\Program Files\Mozilla Build\sccache.exe`,
    expected: "unknown",
  },
  { executable: "-gcc", expected: "unknown" },
  { executable: "-clang", expected: "unknown" },
  { executable: "clang-cl.exe.bak", expected: "unknown" },
  { executable: "cl-wrapper.exe", expected: "unknown" },
  { executable: String.raw`C:\Tools\cl-wrapper.exe`, expected: "unknown" },
  { executable: "/usr/bin/collect2-wrapper", expected: "unknown" },
  { executable: "unknown-compiler", expected: "unknown" },
];

for (const testCase of compilerCases) {
  expectEq(
    identifyCompiler(testCase.executable),
    testCase.expected,
    testCase.executable,
  );
}

const uppercaseCompilerCases: readonly {
  executable: string;
  windowsExpected: CompilerKind;
}[] = [
  {
    executable: String.raw`C:\LLVM\BIN\CLANG.EXE`,
    windowsExpected: "clang",
  },
  {
    executable: String.raw`C:\LLVM\BIN\CLANG-CL.EXE`,
    windowsExpected: "clang-cl",
  },
  { executable: "CL.EXE", windowsExpected: "msvc" },
  {
    executable: String.raw`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\NVCC.EXE`,
    windowsExpected: "nvcc",
  },
];

for (const testCase of uppercaseCompilerCases) {
  expectEq(
    identifyCompiler(testCase.executable),
    platform() === "windows" ? testCase.windowsExpected : "unknown",
    `${testCase.executable} case sensitivity`,
  );
}

const identifier = new CompilerIdentifier();
const versionedClang = identifier.identifyCompilerCommand({
  exe: "clang-20",
  argv: ["clang-20", "-c", "main.cc"],
});
expectEq(versionedClang.key, "builtin:clang", "versioned clang key");
expectEq(versionedClang.dialect, "clang", "versioned clang dialect");
expectEq(versionedClang.target, undefined, "versioned clang target");

const crossClang = identifier.identifyCompilerCommand({
  exe: "/opt/bin/aarch64-linux-gnu-clang-20",
  argv: ["aarch64-linux-gnu-clang-20", "-c", "main.cc"],
});
expectEq(crossClang.key, "builtin:clang", "cross clang key");
expectEq(crossClang.dialect, "clang", "cross clang dialect");
expectEq(
  crossClang.target?.target.triple,
  "aarch64-linux-gnu",
  "cross clang target",
);
expectEq(
  crossClang.target?.source.kind,
  "executable-prefix",
  "cross clang target source",
);

const gccInternal = identifier.identifyCompilerCommand({
  exe: "/usr/libexec/gcc/x86_64-linux-gnu/15/cc1plus",
  argv: ["cc1plus", "main.cc"],
});
expectEq(gccInternal.key, "builtin:unknown", "gcc internal key");
expectEq(gccInternal.dialect, "unknown", "gcc internal dialect");

const gfortran = identifier.identifyCompilerCommand({
  exe: "aarch64-linux-gnu-gfortran-15",
  argv: ["aarch64-linux-gnu-gfortran-15", "-c", "main.f90"],
});
expectEq(gfortran.key, "builtin:unknown", "gfortran family key");
expectEq(gfortran.dialect, "unknown", "gfortran dialect");
expectEq(gfortran.target, undefined, "gfortran target");

identifier.registerCompilerRule("project-clang", {
  match: /(?:^|[\\/])clang$/,
  dialect: CompilerDialect.Msvc,
});
const custom = identifier.identifyCompilerCommand({
  exe: "/project/bin/clang",
  argv: ["clang", "/c", "main.cc"],
});
expectEq(custom.key, "project-clang", "custom rule key");
expectEq(custom.dialect, "msvc", "custom rule dialect");
