module;

#include "modules/prelude.h"

module catter;

import catter.command;

using namespace catter;

namespace {

struct Case {
    std::string_view executable;

    std::optional<Compiler> compiler;
};

/// Executables and the compiler each is; none for a program that is no compiler catter knows.
constexpr Case compilers[] = {
    {.executable = "gcc",                                                                                                     .compiler = Compiler::GCC    },
    {.executable = "g++",                                                                                                     .compiler = Compiler::GCC    },
    {.executable = "gcc-10",                                                                                                  .compiler = Compiler::GCC    },
    {.executable = "g++-10.2",                                                                                                .compiler = Compiler::GCC    },
    {.executable = "/usr/bin/gcc",                                                                                            .compiler = Compiler::GCC    },
    {.executable = "/usr/bin/g++",                                                                                            .compiler = Compiler::GCC    },
    {.executable = "/usr/bin/x86_64-linux-gnu-gcc-13",                                                                        .compiler = Compiler::GCC    },
    {.executable = "/usr/bin/aarch64-linux-gnu-c++",                                                                          .compiler = Compiler::GCC    },
    {.executable = "/usr/local/gcc-15.1.0/bin/c++",                                                                           .compiler = Compiler::GCC    },
    {.executable = "/usr/local/gcc-15.1.0/libexec/gcc/x86_64-pc-linux-gnu/15.1.0/cc1plus",
     .compiler = std::nullopt                                                                                                                              },
    {.executable = "cc1",                                                                                                     .compiler = std::nullopt     },
    {.executable = "f951",                                                                                                    .compiler = std::nullopt     },
    {.executable = "collect2",                                                                                                .compiler = std::nullopt     },
    {.executable = "lto1",                                                                                                    .compiler = std::nullopt     },
    {.executable = R"(C:\msys64\ucrt64\bin\gcc.exe)",                                                                         .compiler = Compiler::GCC    },
    {.executable = R"(C:\msys64\ucrt64\bin\g++.exe)",                                                                         .compiler = Compiler::GCC    },
    {.executable = "C:/msys64/ucrt64/bin/x86_64-w64-mingw32-g++.exe",                                                         .compiler = Compiler::GCC    },
    {.executable = "clang",                                                                                                   .compiler = Compiler::Clang  },
    {.executable = "clang++",                                                                                                 .compiler = Compiler::Clang  },
    {.executable = "clang-12",                                                                                                .compiler = Compiler::Clang  },
    {.executable = "clang++-20",                                                                                              .compiler = Compiler::Clang  },
    {.executable = "/usr/bin/clang",                                                                                          .compiler = Compiler::Clang  },
    {.executable = "/usr/bin/clang++",                                                                                        .compiler = Compiler::Clang  },
    {.executable = "/opt/homebrew/opt/llvm/bin/clang++",                                                                      .compiler = Compiler::Clang  },
    {.executable = "/opt/llvm-20/bin/aarch64-apple-darwin23-clang++",                                                         .compiler = Compiler::Clang  },
    {.executable = R"(D:\LLVM\bin\clang.exe)",                                                                                .compiler = Compiler::Clang  },
    {.executable = R"(D:\LLVM\bin\clang++.exe)",                                                                              .compiler = Compiler::Clang  },
    {.executable = "C:/Program Files/LLVM/bin/clang.exe",                                                                     .compiler = Compiler::Clang  },
    {.executable = "clang-cl",                                                                                                .compiler = Compiler::ClangCL},
    {.executable = "clang-cl.exe",                                                                                            .compiler = Compiler::ClangCL},
    {.executable = "clang-cl-18",                                                                                             .compiler = Compiler::ClangCL},
    {.executable = "clang-cl_20.1",                                                                                           .compiler = Compiler::ClangCL},
    {.executable = R"(C:\Program Files\LLVM\bin\clang-cl.exe)",                                                               .compiler = Compiler::ClangCL},
    {.executable = R"(D:\LLVM\bin\clang-cl.exe)",                                                                             .compiler = Compiler::ClangCL},
    {.executable = "C:/Program Files/LLVM/bin/clang-cl.exe",                                                                  .compiler = Compiler::ClangCL},
    {.executable = "x86_64-pc-windows-msvc-clang-cl.exe",                                                                     .compiler = Compiler::ClangCL},
    {.executable = "cl",                                                                                                      .compiler = Compiler::MSVC   },
    {.executable = "cl.exe",                                                                                                  .compiler = Compiler::MSVC   },
    {.executable = R"(C:\Program Files\Microsoft Visual Studio\VC\Tools\MSVC\bin\cl.exe)",
     .compiler = Compiler::MSVC                                                                                                                            },
    {.executable = R"(D:\MSVC\BuildTools\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe)",
     .compiler = Compiler::MSVC                                                                                                                            },
    {.executable =
         R"(C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe)",
     .compiler = Compiler::MSVC                                                                                                                            },
    {.executable =
         "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe",
     .compiler = Compiler::MSVC                                                                                                                            },
    {.executable =
         R"(C:\PROGRA~1\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe)",
     .compiler = Compiler::MSVC                                                                                                                            },
    {.executable = "gfortran",                                                                                                .compiler = std::nullopt     },
    {.executable = "egfortran",                                                                                               .compiler = std::nullopt     },
    {.executable = "f95",                                                                                                     .compiler = std::nullopt     },
    {.executable = "aarch64-linux-gnu-gfortran-15",                                                                           .compiler = std::nullopt     },
    {.executable = "flang",                                                                                                   .compiler = std::nullopt     },
    {.executable = "flang-new",                                                                                               .compiler = std::nullopt     },
    {.executable = "/opt/llvm/bin/flang-new",                                                                                 .compiler = std::nullopt     },
    {.executable = "aarch64-linux-gnu-flang-19",                                                                              .compiler = std::nullopt     },
    {.executable = "ifort",                                                                                                   .compiler = std::nullopt     },
    {.executable = "ifx",                                                                                                     .compiler = std::nullopt     },
    {.executable = R"(C:\Program Files (x86)\Intel\oneAPI\compiler\latest\bin\ifx.exe)",
     .compiler = std::nullopt                                                                                                                              },
    {.executable = "crayftn",                                                                                                 .compiler = std::nullopt     },
    {.executable = "ftn",                                                                                                     .compiler = std::nullopt     },
    {.executable = "/opt/cray/pe/craype/default/bin/ftn",                                                                     .compiler = std::nullopt     },
    {.executable = "nvcc",                                                                                                    .compiler = Compiler::NVCC   },
    {.executable = "nvcc-12.6",                                                                                               .compiler = Compiler::NVCC   },
    {.executable = "/usr/local/cuda/bin/nvcc",                                                                                .compiler = Compiler::NVCC   },
    {.executable = R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\nvcc.exe)",
     .compiler = Compiler::NVCC                                                                                                                            },
    {.executable = "ccache",                                                                                                  .compiler = std::nullopt     },
    {.executable = "distcc",                                                                                                  .compiler = std::nullopt     },
    {.executable = "sccache",                                                                                                 .compiler = std::nullopt     },
    {.executable = "/usr/lib/ccache/ccache",                                                                                  .compiler = std::nullopt     },
    {.executable = R"(C:\Program Files\Mozilla Build\sccache.exe)",                                                           .compiler = std::nullopt     },
    {.executable = "-gcc",                                                                                                    .compiler = std::nullopt     },
    {.executable = "-clang",                                                                                                  .compiler = std::nullopt     },
    {.executable = "clang-cl.exe.bak",                                                                                        .compiler = std::nullopt     },
    {.executable = "cl-wrapper.exe",                                                                                          .compiler = std::nullopt     },
    {.executable = R"(C:\Tools\cl-wrapper.exe)",                                                                              .compiler = std::nullopt     },
    {.executable = "/usr/bin/collect2-wrapper",                                                                               .compiler = std::nullopt     },
    {.executable = "unknown-compiler",                                                                                        .compiler = std::nullopt     },
};

/// Uppercase executables and the compiler each is on Windows, where names match without case.
constexpr Case uppercase_compilers[] = {
    {.executable = R"(C:\LLVM\BIN\CLANG.EXE)",                                                 .compiler = Compiler::Clang  },
    {.executable = R"(C:\LLVM\BIN\CLANG-CL.EXE)",                                              .compiler = Compiler::ClangCL},
    {.executable = "CL.EXE",                                                                   .compiler = Compiler::MSVC   },
    {.executable = R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\NVCC.EXE)",
     .compiler = Compiler::NVCC                                                                                             },
};

void expect_identified(std::string_view executable, std::optional<Compiler> compiler) {
    ZEST_CONTEXT("executable: {}", executable);
    const std::vector<std::string> argv = {std::string(executable), "-c", "a.c"};
    auto analysis = analyze_compiler(executable, argv);
    const bool nvcc = compiler == Compiler::NVCC;
    if(compiler.has_value() && !nvcc) {
        ZASSERT(analysis.has_value());
        ZEXPECT(analysis->identity.compiler == *compiler);
        return;
    }
    ZASSERT(!analysis.has_value());
    ZEXPECT(analysis.error().kind == Error::Kind::CompilerUnsupported);
    ZEXPECT(analysis.error().message.contains("nvcc") == nvcc);
}

}  // namespace

ZEST_SUITE(cmd_identify_tests) {

ZEST_CASE(executables_are_identified_by_name) {
    for(const auto& [executable, compiler]: compilers) {
        expect_identified(executable, compiler);
    }
};

ZEST_CASE(uppercase_names_match_only_on_windows) {
    for(const auto& entry: uppercase_compilers) {
#ifdef CATTER_WINDOWS
        expect_identified(entry.executable, entry.compiler);
#else
        expect_identified(entry.executable, std::nullopt);
#endif
    }
};

ZEST_CASE(versioned_clang_names_no_target) {
    const std::vector<std::string> argv = {"clang-20", "-c", "main.cc"};
    auto analysis = analyze_compiler(argv[0], argv);
    ZASSERT(analysis.has_value());
    ZEXPECT(analysis->identity.compiler == Compiler::Clang);
    ZEXPECT(analysis->identity.dialect == Dialect::Clang);
    ZEXPECT(analysis->identity.prefix.empty());
    ZEXPECT(analysis->target.source != TargetSource::ExecutablePrefix);
    ZEXPECT(!analysis->target.triple.has_value());
};

ZEST_CASE(prefixed_clang_names_its_target) {
    const std::vector<std::string> argv = {"aarch64-linux-gnu-clang-20", "-c", "main.cc"};
    auto analysis = analyze_compiler("/opt/bin/aarch64-linux-gnu-clang-20", argv);
    ZASSERT(analysis.has_value());
    ZEXPECT(analysis->identity.compiler == Compiler::Clang);
    ZEXPECT(analysis->identity.dialect == Dialect::Clang);
    ZEXPECT(analysis->target.triple == "aarch64-linux-gnu");
    ZEXPECT(analysis->target.source == TargetSource::ExecutablePrefix);
};

ZEST_CASE(gcc_internal_is_unsupported) {
    const std::vector<std::string> argv = {"cc1plus", "main.cc"};
    auto analysis = analyze_compiler("/usr/libexec/gcc/x86_64-linux-gnu/15/cc1plus", argv);
    ZASSERT(!analysis.has_value());
    ZEXPECT(analysis.error().kind == Error::Kind::CompilerUnsupported);
    ZEXPECT(!analysis.error().message.contains("nvcc"));
};

};  // ZEST_SUITE(cmd_identify_tests)
