#include <optional>

#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "compiler.h"

using namespace catter;

TEST_SUITE(compiler_tests) {
TEST_CASE(identify_compiler) {
    struct TestCase {
        std::string_view input;
        Compiler expected;
    };

    std::vector<TestCase> test_cases = {
        {"gcc",                                                                  Compiler::gcc    },
        {"g++",                                                                  Compiler::gcc    },
        {"gcc-10",                                                               Compiler::gcc    },
        {"g++-10.2",                                                             Compiler::gcc    },
        {"/usr/local/gcc-15.1.0/bin/c++",                                        Compiler::gcc    },
        {"/usr/local/gcc-15.1.0/libexec/gcc/x86_64-pc-linux-gnu/15.1.0/cc1plus", Compiler::gcc    },
        {"clang",                                                                Compiler::clang  },
        {"clang++",                                                              Compiler::clang  },
        {"clang-12",                                                             Compiler::clang  },
        {"flang",                                                                Compiler::flang  },
        {"ifort",                                                                Compiler::ifort  },
        {"ifx",                                                                  Compiler::ifort  },
        {"crayftn",                                                              Compiler::crayftn},
        {"nvcc",                                                                 Compiler::nvcc   },
        {"ccache",                                                               Compiler::wrapper},
        {"distcc",                                                               Compiler::wrapper},
        {"sccache",                                                              Compiler::wrapper},
        {"unknown-compiler",                                                     Compiler::unknown}
    };

    for(const auto& test_case: test_cases) {
        EXPECT_EQ(test_case.expected, identify_compiler(test_case.input));
    }
};
};  // TEST_SUITE(compiler_tests)
