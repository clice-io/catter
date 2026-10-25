#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "command/compiler.h"
#include "support/encoding.h"

using namespace catter;

namespace {

struct Case {
    struct Target {
        cmd::ArtifactModel model;

        cmd::TargetSource source;

        std::optional<std::string> triple;
    };

    std::string_view label;

    std::vector<std::string> argv;

    cmd::Phase phase;

    cmd::Artifact artifact;

    std::vector<std::string> reads;

    std::vector<std::string> writes;

    std::optional<Target> target;
};

std::string join(std::string_view directory, std::string_view name) {
    return util::to_utf8((util::from_utf8(directory) / util::from_utf8(name)).lexically_normal());
}

std::vector<std::string> paths(const auto& files) {
    return files | std::views::transform([](const auto& file) { return file.path; }) |
           std::ranges::to<std::vector>();
}

/// Commands with their mode, the files they read and write, in order, and some with their target.
const Case cases[] = {
    {
     .label = "clang llvm ir explicit stdout output",
     .argv = {"clang", "src/t.c", "-S", "-emit-llvm", "-o", "-"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::LLVMIR,
     .reads = {"src/t.c"},
     .writes = {"-"},
     },
    {
     .label = "clang stdin input is not a filesystem read",
     .argv = {"clang", "-x", "c", "-c", "-", "-o", "stdin.o"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {},
     .writes = {"stdin.o"},
     },
    {
     .label = "clang stdin input has no default filesystem output",
     .argv = {"clang", "-x", "c", "-c", "-"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {},
     .writes = {},
     },
    {
     .label = "gcc preprocess explicit language rejects no-suffix candidate",
     .argv = {"gcc", "-x", "c", "not-a-source", "src/a.c", "-E", "-P"},
     .phase = cmd::Phase::Preprocess,
     .artifact = cmd::Artifact::PreprocessedSource,
     .reads = {"src/a.c"},
     .writes = {},
     },
    {
     .label = "gcc preprocess to file",
     .argv = {"gcc", "-E", "src/a.c", "-o", "a.i"},
     .phase = cmd::Phase::Preprocess,
     .artifact = cmd::Artifact::PreprocessedSource,
     .reads = {"src/a.c"},
     .writes = {"a.i"},
     },
    {
     .label = "gcc syntax-only explicit language",
     .argv = {"gcc", "-x", "c++", "not-a-source", "-fsyntax-only", "-fno-exceptions"},
     .phase = cmd::Phase::SyntaxOnly,
     .artifact = cmd::Artifact::None,
     .reads = {},
     .writes = {},
     },
    {
     .label = "gcc x-none resets classification for later object inputs",
     .argv = {"gcc", "-x", "c", "not-a-source", "-x", "none", "obj/plain.o", "-o", "bin/app"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"obj/plain.o"},
     .writes = {"bin/app"},
     },
    {
     .label = "gcc relocatable link with extra linker flags",
     .argv = {"gcc", "-nostdlib", "-Wl,--build-id=sha1", "-r", "a.o", "b.o", "-o", "partial.o"},
     .phase = cmd::Phase::RelocatableLink,
     .artifact = cmd::Artifact::Object,
     .reads = {"a.o", "b.o"},
     .writes = {"partial.o"},
     },
    {
     .label = "gcc compile link input has no default source output",
     .argv = {"gcc", "-x", "none", "obj/plain.o", "-c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"obj/plain.o"},
     .writes = {},
     },
    {
     .label = "gcc joined output spelling",
     .argv = {"gcc", "-c", "src/joined.c", "-oobj/joined.o"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/joined.c"},
     .writes = {"obj/joined.o"},
     },
    {
     .label = "clang llvm bitcode action survives object stop phase",
     .argv = {"clang", "-emit-llvm", "-c", "src/t.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::LLVMBitcode,
     .reads = {"src/t.c"},
     .writes = {"t.bc"},
     },
    {
     .label = "clang assembly action survives object stop phase",
     .argv = {"clang", "-S", "-c", "src/t.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Assembly,
     .reads = {"src/t.c"},
     .writes = {"t.s"},
     },
    {
     .label = "clang compile action survives shared link option",
     .argv = {"clang", "-c", "-shared", "src/t.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/t.c"},
     .writes = {"t.o"},
     },
    {
     .label = "clang preprocess unknown option value without suffix is not an input",
     .argv = {"clang",
                 "-E",
                 "--definitely-not-a-real-clang-flag",
                 "not-a-source",
                 "src/main.c",
                 "-o",
                 "main.i"},
     .phase = cmd::Phase::Preprocess,
     .artifact = cmd::Artifact::PreprocessedSource,
     .reads = {"src/main.c"},
     .writes = {"main.i"},
     },
    {
     .label = "clang unknown option value without suffix is not an input",
     .argv = {"clang", "-c", "--definitely-not-a-real-clang-flag", "not-a-source", "src/main.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/main.c"},
     .writes = {"main.o"},
     },
    {
     .label = "clang link unknown option value without suffix is not an input",
     .argv = {"clang",
                 "--definitely-not-a-real-clang-flag",
                 "not-a-source",
                 "obj/main.o",
                 "-o",
                 "bin/app"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"obj/main.o"},
     .writes = {"bin/app"},
     },
    {
     .label = "gcc unknown option value without suffix is not an input",
     .argv = {"gcc",
                 "--definitely-not-a-real-gcc-flag",
                 "not-a-source",
                 "src/main.c",
                 "-o",
                 "bin/app"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"src/main.c"},
     .writes = {"bin/app"},
     },
    {
     .label = "gcc explicit language rejects unknown option value candidate",
     .argv = {"gcc",
                 "-x",
                 "c",
                 "--definitely-not-a-real-gcc-flag",
                 "not-a-source",
                 "src/main.c",
                 "-c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/main.c"},
     .writes = {"main.o"},
     },
    {
     .label = "clang unresolved default executable output",
     .argv = {"clang", "src/t.c"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"src/t.c"},
     .writes = {},
     },
    {
     .label = "clang unresolved default executable output from object input",
     .argv = {"clang", "obj/t.o"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"obj/t.o"},
     .writes = {},
     },
    {
     .label = "clang archive static lib from object inputs",
     .argv = {"clang", "--emit-static-lib", "a.o", "b.o", "-o", "libstuff.a"},
     .phase = cmd::Phase::Archive,
     .artifact = cmd::Artifact::StaticLibrary,
     .reads = {"a.o", "b.o"},
     .writes = {"libstuff.a"},
     },
    {
     .label = "clang compile multiple translation units with default outputs",
     .argv = {"clang", "-c", "src/a.c", "src/b.cc"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/a.c", "src/b.cc"},
     .writes = {"a.o", "b.o"},
     },
    {
     .label = "clang explicit windows msvc target object output",
     .argv = {"clang", "--target=x86_64-pc-windows-msvc", "-c", "main.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"main.c"},
     .writes = {"main.obj"},
     .target = Case::Target{.model = cmd::ArtifactModel::COFFMSVC,
                               .source = cmd::TargetSource::Argument,
                               .triple = "x86_64-pc-windows-msvc"},
     },
    {
     .label = "prefixed clang windows msvc target",
     .argv = {"x86_64-pc-windows-msvc-clang", "-c", "main.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"main.c"},
     .writes = {"main.obj"},
     .target = Case::Target{.model = cmd::ArtifactModel::COFFMSVC,
                               .source = cmd::TargetSource::ExecutablePrefix,
                               .triple = "x86_64-pc-windows-msvc"},
     },
    {
     .label = "explicit clang target overrides executable target",
     .argv = {"aarch64-linux-gnu-clang", "--target=x86_64-pc-windows-msvc", "-c", "main.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"main.c"},
     .writes = {"main.obj"},
     .target = Case::Target{.model = cmd::ArtifactModel::COFFMSVC,
                               .source = cmd::TargetSource::Argument,
                               .triple = "x86_64-pc-windows-msvc"},
     },
    {
     .label = "versioned clang cross compiler target",
     .argv = {"aarch64-linux-gnu-clang-20", "-c", "main.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"main.c"},
     .writes = {"main.o"},
     .target = Case::Target{.model = cmd::ArtifactModel::ELF,
                               .source = cmd::TargetSource::ExecutablePrefix,
                               .triple = "aarch64-linux-gnu"},
     },
    {
     .label = "clang unresolved windows gnu target executable output",
     .argv = {"clang", "--target=x86_64-w64-windows-gnu", "src/tool.c"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"src/tool.c"},
     .writes = {},
     },
    {
     .label = "clang windows msvc target accepts coff link inputs",
     .argv = {"clang", "--target=x86_64-pc-windows-msvc", "obj/main.obj", "-o", "bin/app.exe"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"obj/main.obj"},
     .writes = {"bin/app.exe"},
     },
    {
     .label = "clang linux target rejects coff link input suffix",
     .argv = {"clang", "--target=x86_64-unknown-linux-gnu", "obj/main.obj", "-o", "bin/app"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {},
     .writes = {"bin/app"},
     },
    {
     .label = "clang unresolved linux target executable output",
     .argv = {"clang", "--target=x86_64-unknown-linux-gnu", "src/tool.c"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"src/tool.c"},
     .writes = {},
     },
    {
     .label = "prefixed mingw gnu driver object output",
     .argv = {"x86_64-w64-mingw32-g++", "-c", "main.cpp"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"main.cpp"},
     .writes = {"main.o"},
     },
    {
     .label = "clang-cl cl-style compile no suffix into object dir",
     .argv = {"clang-cl", "/c", "/Tp", "src/noext", "/Fo:build/"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/noext"},
     .writes = {join("build", "noext.obj")},
     },
    {
     .label = "clang-cl explicit linux target object output",
     .argv = {"clang-cl", "--target=x86_64-linux-gnu", "/c", "main.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"main.c"},
     .writes = {"main.o"},
     .target = Case::Target{.model = cmd::ArtifactModel::ELF,
                               .source = cmd::TargetSource::Argument,
                               .triple = "x86_64-linux-gnu"},
     },
    {
     .label = "clang-cl cl-style default executable output",
     .argv = {"clang-cl", "src/main.cpp"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"src/main.cpp"},
     .writes = {"main.exe"},
     .target = Case::Target{.model = cmd::ArtifactModel::COFFMSVC,
                               .source = cmd::TargetSource::DriverDefault,
                               .triple = "unknown-pc-windows-msvc"},
     },
    {
     .label = "clang-cl dash preprocess to file mode",
     .argv = {"clang-cl", "-P", "src/main.cpp"},
     .phase = cmd::Phase::Preprocess,
     .artifact = cmd::Artifact::PreprocessedSource,
     .reads = {"src/main.cpp"},
     .writes = {},
     },
    {
     .label = "clang-cl assembly listing does not stop link",
     .argv = {"clang-cl", "/FA", "src/main.cpp"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::Executable,
     .reads = {"src/main.cpp"},
     .writes = {"main.exe", "main.asm"},
     },
    {
     .label = "clang-cl assembly listing directory output",
     .argv = {"clang-cl", "/c", "/FA", "/Faasm/", "src/main.cpp"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/main.cpp"},
     .writes = {"main.obj", join("asm", "main.asm")},
     },
    {
     .label = "clang-cl assembly listing single file output",
     .argv = {"clang-cl", "/c", "/Faasm.lst", "src/main.cpp"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/main.cpp"},
     .writes = {"main.obj", "asm.lst"},
     },
    {
     .label = "clang-cl cl-style default shared library output",
     .argv = {"clang-cl", "/LD", "src/plugin.cpp"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::SharedLibrary,
     .reads = {"src/plugin.cpp"},
     .writes = {"plugin.dll"},
     },
    {
     .label = "msvc cl-style compile explicit object output",
     .argv = {"cl.exe", "/c", "src/main.cpp", "/Foobj/main.obj"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/main.cpp"},
     .writes = {"obj/main.obj"},
     },
    {
     .label = "msvc cl-style shared output directory",
     .argv = {"cl.exe", "/LD", "src/plugin.cpp", "/Fe:bin/"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::SharedLibrary,
     .reads = {"src/plugin.cpp"},
     .writes = {join("bin", "plugin.dll")},
     },
    {
     .label = "msvc cl-style shared link via linker remainder",
     .argv = {"cl.exe", "/link", "/dll", "/out:bin/tool.dll", "foo.obj", "bar.res"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::SharedLibrary,
     .reads = {"foo.obj", "bar.res"},
     .writes = {"bin/tool.dll"},
     },
    {
     .label = "clang driver-mode cl compile no suffix into object dir",
     .argv = {"clang", "--driver-mode=cl", "/c", "/Tp", "src/noext", "/Fo:build/"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"src/noext"},
     .writes = {join("build", "noext.obj")},
     },
    {
     .label = "clang driver-mode cl shared output directory",
     .argv = {"clang", "--driver-mode=cl", "/LD", "src/plugin.cpp", "/Fe:bin/"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::SharedLibrary,
     .reads = {"src/plugin.cpp"},
     .writes = {join("bin", "plugin.dll")},
     },
    {
     .label = "clang driver-mode cl assembly listing keeps shared link mode",
     .argv = {"clang",
                 "--driver-mode=cl",
                 "/FA",
                 "src/plugin.cpp",
                 "/link",
                 "/dll",
                 "/out:bin/plugin.dll"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::SharedLibrary,
     .reads = {"src/plugin.cpp"},
     .writes = {"bin/plugin.dll", "plugin.asm"},
     },
    {
     .label = "clang driver-mode cl default object output",
     .argv = {"clang", "--driver-mode=cl", "-c", "main.c"},
     .phase = cmd::Phase::Compile,
     .artifact = cmd::Artifact::Object,
     .reads = {"main.c"},
     .writes = {"main.obj"},
     },
    {
     .label = "clang driver-mode cl parses linker remainder after input",
     .argv = {"clang",
                 "--driver-mode=cl",
                 "foo.obj",
                 "/link",
                 "/dll",
                 "/out:bin/tool.dll",
                 "bar.res"},
     .phase = cmd::Phase::Link,
     .artifact = cmd::Artifact::SharedLibrary,
     .reads = {"foo.obj", "bar.res"},
     .writes = {"bin/tool.dll"},
     },
};

void expect_analysis(const Case& expected) {
    ZEST_CONTEXT("case: {}", expected.label);
    auto analysis = cmd::analyze_compiler(expected.argv[0], expected.argv);
    ASSERT(analysis.has_value());
    EXPECT(analysis->phase == expected.phase);
    EXPECT(analysis->artifact == expected.artifact);
    EXPECT(paths(analysis->reads) == expected.reads);
    EXPECT(paths(analysis->writes) == expected.writes);
    if(expected.target.has_value()) {
        EXPECT(analysis->target.model == expected.target->model);
        EXPECT(analysis->target.source == expected.target->source);
        EXPECT(analysis->target.triple == expected.target->triple);
    }
}

}  // namespace

ZEST_SUITE(cmd_analysis_tests) {

ZEST_CASE(commands_have_their_mode_and_files) {
    for(const auto& expected: cases) {
        expect_analysis(expected);
    }
};

ZEST_CASE(compilers_of_each_dialect_are_analyzed) {
    const std::vector<std::string> commands[] = {
        {"clang",    "-c", "main.cc"},
        {"gcc",      "-c", "main.cc"},
        {"clang-cl", "/c", "main.cc"},
        {"cl.exe",   "/c", "main.cc"},
    };
    for(const auto& argv: commands) {
        ZEST_CONTEXT("executable: {}", argv[0]);
        EXPECT(cmd::analyze_compiler(argv[0], argv).has_value());
    }
};

ZEST_CASE(nvcc_is_unsupported) {
    const std::vector<std::string> argv = {"nvcc", "-c", "kernel.cu"};
    auto analysis = cmd::analyze_compiler(argv[0], argv);
    ASSERT(!analysis.has_value());
    EXPECT(analysis.error().kind == cmd::Error::Kind::CompilerUnsupported);
    EXPECT(analysis.error().message.contains("nvcc"));
};

ZEST_CASE(clang_cl_rejects_gnu_modes) {
    const std::vector<std::string> commands[] = {
        {"clang-cl", "-S", "src/main.cpp"},
        {"clang-cl", "-emit-llvm", "-c", "src/main.cpp"},
    };
    for(const auto& argv: commands) {
        ZEST_CONTEXT("option: {}", argv[1]);
        EXPECT(!cmd::analyze_compiler(argv[0], argv).has_value());
    }
};

ZEST_CASE(last_target_argument_wins) {
    const std::vector<std::string> argv = {"clang",
                                           "--target=aarch64-linux-gnu",
                                           "--target=x86_64-pc-windows-msvc",
                                           "-c",
                                           "main.c"};
    auto analysis = cmd::analyze_compiler(argv[0], argv);
    ASSERT(analysis.has_value());
    EXPECT(analysis->target.triple == "x86_64-pc-windows-msvc");
    EXPECT(analysis->target.source == cmd::TargetSource::Argument);
};

ZEST_CASE(absolute_executable_names_default_output) {
    const std::vector<std::string> argv = {"gcc", "-c", "absolute.c"};
    auto analysis = cmd::analyze_compiler("/opt/toolchains/bin/gcc", argv);
    ASSERT(analysis.has_value());
    EXPECT(paths(analysis->writes) == std::vector<std::string>{"absolute.o"});
};

ZEST_CASE(absolute_cross_clang_names_its_target) {
    const std::vector<std::string> argv = {"x86_64-w64-mingw32-clang++.exe", "-c", "main.cpp"};
    auto analysis = cmd::analyze_compiler("C:/toolchains/bin/x86_64-w64-mingw32-clang++.exe", argv);
    ASSERT(analysis.has_value());
    EXPECT(analysis->target.triple == "x86_64-w64-mingw32");
};

};  // ZEST_SUITE(cmd_analysis_tests)
