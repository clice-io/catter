#include "cmd/compiler.h"

#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

using namespace catter;

namespace {

std::vector<std::string> split(std::string_view command) {
    return command | std::views::split(' ') | std::ranges::to<std::vector<std::string>>();
}

/// Commands of every dialect: options that take their value joined and separate, a directory
/// output, inputs after `--` and after `/link`, joined to it or not, an output the phase does not
/// take.
constexpr std::string_view commands[] = {
    "gcc -c -Iinclude -isystem /usr/include a.c -o a.o",
    "clang++ -x c++ -c a.cc b.cc -o out/ -- -dash.cc",
    "clang --driver-mode=cl /c /Foout\\ a.cc /Tcb.c /Tc c.c",
    "cl /nologo /EHsc /Tp a.cc /Fea.exe b.cc /link /out:x.exe lib.lib /dll",
    "cl /c a.cc /FA /link /out x.exe",
    "cl /nologo a.cc /link/out:x.exe lib.lib",
    "clang-cl /c a.cc /link/out:a.exe",
    "gcc a.o b.o -o app -lm",
};

}  // namespace

ZEST_SUITE(cmd_compiler_tests) {

ZEST_CASE(arguments_cover_argv) {
    for(auto command: commands) {
        const auto argv = split(command);
        auto analysis = cmd::analyze_compiler(argv[0], argv);
        ASSERT(analysis.has_value());

        std::uint32_t next = 1;
        for(const auto& argument: analysis->arguments) {
            if(argv[next] == "--") {
                next += 1;
            }
            EXPECT(argument.index == next);
            EXPECT(argument.count >= 1);
            next = argument.index + argument.count;
        }
        EXPECT(next == argv.size());
    }
};

ZEST_CASE(locations_point_at_their_paths) {
    for(auto command: commands) {
        const auto argv = split(command);
        auto analysis = cmd::analyze_compiler(argv[0], argv);
        ASSERT(analysis.has_value());

        const auto names = [&](const cmd::Location& location, std::string_view path) {
            const auto& argument = analysis->arguments[location.argument];
            return location.index >= argument.index &&
                   location.index < argument.index + argument.count &&
                   argv[location.index].ends_with(path);
        };
        for(const auto& read: analysis->reads) {
            EXPECT(names(read.location, read.path));
        }
        for(const auto& diagnostic: analysis->diagnostics) {
            if(diagnostic.location.has_value()) {
                EXPECT(names(*diagnostic.location, *diagnostic.path));
            }
        }
    }
};

ZEST_CASE(writes_are_made_from_reads_in_argv_order) {
    for(auto command: commands) {
        const auto argv = split(command);
        auto analysis = cmd::analyze_compiler(argv[0], argv);
        ASSERT(analysis.has_value());

        EXPECT(std::ranges::is_sorted(analysis->reads, {}, [](const cmd::Read& read) {
            return read.location.index;
        }));
        for(const auto& write: analysis->writes) {
            for(auto read: write.reads) {
                EXPECT(read < analysis->reads.size());
            }
        }
    }
};

ZEST_CASE(missing_option_value_fails_the_parse) {
    const auto argv = split("gcc -c a.c -o");
    auto analysis = cmd::analyze_compiler(argv[0], argv);
    ASSERT(!analysis.has_value());
    EXPECT(analysis.error().kind == cmd::Error::Kind::CompilerParse);
};

};  // ZEST_SUITE(cmd_compiler_tests)
