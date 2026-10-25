#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "command/compiler.h"

using namespace catter;

namespace {

struct Case {
    std::string_view executable;

    std::optional<cmd::Compiler> compiler;
};

/// Executables and the compiler each is; none for a program that is no compiler catter knows.
constexpr Case compilers[] = {
    {.executable = "gcc",                                                                                                     .compiler = cmd::Compiler::GCC    },
    {.executable = "g++",                                                                                                     .compiler = cmd::Compiler::GCC    },
    {.executable = "gcc-10",                                                                                                  .compiler = cmd::Compiler::GCC    },
    {.executable = "g++-10.2",                                                                                                .compiler = cmd::Compiler::GCC    },
    {.executable = "/usr/bin/gcc",                                                                                            .compiler = cmd::Compiler::GCC    },
    {.executable = "/usr/bin/g++",                                                                                            .compiler = cmd::Compiler::GCC    },
    {.executable = "/usr/bin/x86_64-linux-gnu-gcc-13",                                                                        .compiler = cmd::Compiler::GCC    },
    {.executable = "/usr/bin/aarch64-linux-gnu-c++",                                                                          .compiler = cmd::Compiler::GCC    },
    {.executable = "/usr/local/gcc-15.1.0/bin/c++",                                                                           .compiler = cmd::Compiler::GCC    },
    {.executable = "/usr/local/gcc-15.1.0/libexec/gcc/x86_64-pc-linux-gnu/15.1.0/cc1plus",
     .compiler = std::nullopt                                                                                                                                   },
    {.executable = "cc1",                                                                                                     .compiler = std::nullopt          },
    {.executable = "f951",                                                                                                    .compiler = std::nullopt          },
    {.executable = "collect2",                                                                                                .compiler = std::nullopt          },
    {.executable = "lto1",                                                                                                    .compiler = std::nullopt          },
    {.executable = R"(C:\msys64\ucrt64\bin\gcc.exe)",                                                                         .compiler = cmd::Compiler::GCC    },
    {.executable = R"(C:\msys64\ucrt64\bin\g++.exe)",                                                                         .compiler = cmd::Compiler::GCC    },
    {.executable = "C:/msys64/ucrt64/bin/x86_64-w64-mingw32-g++.exe",
     .compiler = cmd::Compiler::GCC                                                                                                                             },
    {.executable = "clang",                                                                                                   .compiler = cmd::Compiler::Clang  },
    {.executable = "clang++",                                                                                                 .compiler = cmd::Compiler::Clang  },
    {.executable = "clang-12",                                                                                                .compiler = cmd::Compiler::Clang  },
    {.executable = "clang++-20",                                                                                              .compiler = cmd::Compiler::Clang  },
    {.executable = "/usr/bin/clang",                                                                                          .compiler = cmd::Compiler::Clang  },
    {.executable = "/usr/bin/clang++",                                                                                        .compiler = cmd::Compiler::Clang  },
    {.executable = "/opt/homebrew/opt/llvm/bin/clang++",                                                                      .compiler = cmd::Compiler::Clang  },
    {.executable = "/opt/llvm-20/bin/aarch64-apple-darwin23-clang++",
     .compiler = cmd::Compiler::Clang                                                                                                                           },
    {.executable = R"(D:\LLVM\bin\clang.exe)",                                                                                .compiler = cmd::Compiler::Clang  },
    {.executable = R"(D:\LLVM\bin\clang++.exe)",                                                                              .compiler = cmd::Compiler::Clang  },
    {.executable = "C:/Program Files/LLVM/bin/clang.exe",                                                                     .compiler = cmd::Compiler::Clang  },
    {.executable = "clang-cl",                                                                                                .compiler = cmd::Compiler::ClangCL},
    {.executable = "clang-cl.exe",                                                                                            .compiler = cmd::Compiler::ClangCL},
    {.executable = "clang-cl-18",                                                                                             .compiler = cmd::Compiler::ClangCL},
    {.executable = "clang-cl_20.1",                                                                                           .compiler = cmd::Compiler::ClangCL},
    {.executable = R"(C:\Program Files\LLVM\bin\clang-cl.exe)",                                                               .compiler = cmd::Compiler::ClangCL},
    {.executable = R"(D:\LLVM\bin\clang-cl.exe)",                                                                             .compiler = cmd::Compiler::ClangCL},
    {.executable = "C:/Program Files/LLVM/bin/clang-cl.exe",                                                                  .compiler = cmd::Compiler::ClangCL},
    {.executable = "x86_64-pc-windows-msvc-clang-cl.exe",                                                                     .compiler = cmd::Compiler::ClangCL},
    {.executable = "cl",                                                                                                      .compiler = cmd::Compiler::MSVC   },
    {.executable = "cl.exe",                                                                                                  .compiler = cmd::Compiler::MSVC   },
    {.executable = R"(C:\Program Files\Microsoft Visual Studio\VC\Tools\MSVC\bin\cl.exe)",
     .compiler = cmd::Compiler::MSVC                                                                                                                            },
    {.executable = R"(D:\MSVC\BuildTools\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe)",
     .compiler = cmd::Compiler::MSVC                                                                                                                            },
    {.executable =
         R"(C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe)",
     .compiler = cmd::Compiler::MSVC                                                                                                                            },
    {.executable =
         "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe",
     .compiler = cmd::Compiler::MSVC                                                                                                                            },
    {.executable =
         R"(C:\PROGRA~1\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe)",
     .compiler = cmd::Compiler::MSVC                                                                                                                            },
    {.executable = "gfortran",                                                                                                .compiler = std::nullopt          },
    {.executable = "egfortran",                                                                                               .compiler = std::nullopt          },
    {.executable = "f95",                                                                                                     .compiler = std::nullopt          },
    {.executable = "aarch64-linux-gnu-gfortran-15",                                                                           .compiler = std::nullopt          },
    {.executable = "flang",                                                                                                   .compiler = std::nullopt          },
    {.executable = "flang-new",                                                                                               .compiler = std::nullopt          },
    {.executable = "/opt/llvm/bin/flang-new",                                                                                 .compiler = std::nullopt          },
    {.executable = "aarch64-linux-gnu-flang-19",                                                                              .compiler = std::nullopt          },
    {.executable = "ifort",                                                                                                   .compiler = std::nullopt          },
    {.executable = "ifx",                                                                                                     .compiler = std::nullopt          },
    {.executable = R"(C:\Program Files (x86)\Intel\oneAPI\compiler\latest\bin\ifx.exe)",
     .compiler = std::nullopt                                                                                                                                   },
    {.executable = "crayftn",                                                                                                 .compiler = std::nullopt          },
    {.executable = "ftn",                                                                                                     .compiler = std::nullopt          },
    {.executable = "/opt/cray/pe/craype/default/bin/ftn",                                                                     .compiler = std::nullopt          },
    {.executable = "nvcc",                                                                                                    .compiler = cmd::Compiler::NVCC   },
    {.executable = "nvcc-12.6",                                                                                               .compiler = cmd::Compiler::NVCC   },
    {.executable = "/usr/local/cuda/bin/nvcc",                                                                                .compiler = cmd::Compiler::NVCC   },
    {.executable = R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\nvcc.exe)",
     .compiler = cmd::Compiler::NVCC                                                                                                                            },
    {.executable = "ccache",                                                                                                  .compiler = std::nullopt          },
    {.executable = "distcc",                                                                                                  .compiler = std::nullopt          },
    {.executable = "sccache",                                                                                                 .compiler = std::nullopt          },
    {.executable = "/usr/lib/ccache/ccache",                                                                                  .compiler = std::nullopt          },
    {.executable = R"(C:\Program Files\Mozilla Build\sccache.exe)",                                                           .compiler = std::nullopt          },
    {.executable = "-gcc",                                                                                                    .compiler = std::nullopt          },
    {.executable = "-clang",                                                                                                  .compiler = std::nullopt          },
    {.executable = "clang-cl.exe.bak",                                                                                        .compiler = std::nullopt          },
    {.executable = "cl-wrapper.exe",                                                                                          .compiler = std::nullopt          },
    {.executable = R"(C:\Tools\cl-wrapper.exe)",                                                                              .compiler = std::nullopt          },
    {.executable = "/usr/bin/collect2-wrapper",                                                                               .compiler = std::nullopt          },
    {.executable = "unknown-compiler",                                                                                        .compiler = std::nullopt          },
};

/// Uppercase executables and the compiler each is on Windows, where names match without case.
constexpr Case uppercase_compilers[] = {
    {.executable = R"(C:\LLVM\BIN\CLANG.EXE)",                                                 .compiler = cmd::Compiler::Clang  },
    {.executable = R"(C:\LLVM\BIN\CLANG-CL.EXE)",                                              .compiler = cmd::Compiler::ClangCL},
    {.executable = "CL.EXE",                                                                   .compiler = cmd::Compiler::MSVC   },
    {.executable = R"(C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\NVCC.EXE)",
     .compiler = cmd::Compiler::NVCC                                                                                             },
};

void expect_identified(std::string_view executable, std::optional<cmd::Compiler> compiler) {
    ZEST_CONTEXT("executable: {}", executable);
    const std::vector<std::string> argv = {std::string(executable), "-c", "a.c"};
    auto analysis = cmd::analyze_compiler(executable, argv);
    const bool nvcc = compiler == cmd::Compiler::NVCC;
    if(compiler.has_value() && !nvcc) {
        ASSERT(analysis.has_value());
        EXPECT(analysis->identity.compiler == *compiler);
        return;
    }
    ASSERT(!analysis.has_value());
    EXPECT(analysis.error().kind == cmd::Error::Kind::CompilerUnsupported);
    EXPECT(analysis.error().message.contains("nvcc") == nvcc);
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
    auto analysis = cmd::analyze_compiler(argv[0], argv);
    ASSERT(analysis.has_value());
    EXPECT(analysis->identity.compiler == cmd::Compiler::Clang);
    EXPECT(analysis->identity.dialect == cmd::Dialect::Clang);
    EXPECT(analysis->identity.prefix.empty());
    EXPECT(analysis->target.source != cmd::TargetSource::ExecutablePrefix);
    EXPECT(!analysis->target.triple.has_value());
};

ZEST_CASE(prefixed_clang_names_its_target) {
    const std::vector<std::string> argv = {"aarch64-linux-gnu-clang-20", "-c", "main.cc"};
    auto analysis = cmd::analyze_compiler("/opt/bin/aarch64-linux-gnu-clang-20", argv);
    ASSERT(analysis.has_value());
    EXPECT(analysis->identity.compiler == cmd::Compiler::Clang);
    EXPECT(analysis->identity.dialect == cmd::Dialect::Clang);
    EXPECT(analysis->target.triple == "aarch64-linux-gnu");
    EXPECT(analysis->target.source == cmd::TargetSource::ExecutablePrefix);
};

ZEST_CASE(gcc_internal_is_unsupported) {
    const std::vector<std::string> argv = {"cc1plus", "main.cc"};
    auto analysis = cmd::analyze_compiler("/usr/libexec/gcc/x86_64-linux-gnu/15/cc1plus", argv);
    ASSERT(!analysis.has_value());
    EXPECT(analysis.error().kind == cmd::Error::Kind::CompilerUnsupported);
    EXPECT(!analysis.error().message.contains("nvcc"));
};

};  // ZEST_SUITE(cmd_identify_tests)
