module;

#include "support/prelude.h"

module catter;

import catter.command;

using namespace catter;

ZEST_SUITE(cmd_archiver_tests) {

ZEST_CASE(archivers_are_identified_by_name) {
    auto archiver = [](std::string_view executable) {
        const std::vector<std::string> argv = {std::string(executable), "rcs", "libx.a", "a.o"};
        return analyze_archiver(executable, argv).has_value();
    };
    for(auto executable: {"ar",
                          "/usr/bin/ar",
                          "llvm-ar",
                          "llvm-ar-18",
                          "gcc-ar",
                          "gcc-ar-14",
                          "x86_64-linux-gnu-ar",
                          "x86_64-linux-gnu-gcc-ar-13",
                          "x86_64-conda-linux-gnu-ar",
                          "arm-none-eabi-ar",
                          R"(C:\msys64\ucrt64\bin\ar.exe)"}) {
        ZEST_CONTEXT("executable: {}", executable);
        ZEXPECT(archiver(executable));
    }
    for(auto executable: {"tar", "rar", "ar-wrapper", "arm-none-eabi-gcc"}) {
        ZEST_CONTEXT("executable: {}", executable);
        ZEXPECT(!archiver(executable));
    }
};

};  // ZEST_SUITE(cmd_archiver_tests)
