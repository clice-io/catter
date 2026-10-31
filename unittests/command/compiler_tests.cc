module;

#include "modules/prelude.h"

module catter;

import catter.command;

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
    "gcc -c a.c -dumpbase b.c -wrapper echo,c.c -o a.o",
    "cl /TP -scanDependencies a.ddi -Foa.obj a.cc",
};

}  // namespace

ZEST_SUITE(cmd_compiler_tests) {

ZEST_CASE(arguments_cover_argv) {
    for(auto command: commands) {
        const auto argv = split(command);
        auto analysis = analyze_compiler(argv[0], argv, "", kotatsu::fs::read_file);
        ZASSERT(analysis.has_value());

        std::uint32_t next = 1;
        for(const auto& argument: analysis->arguments) {
            if(argv[next] == "--") {
                next += 1;
            }
            ZEXPECT(argument.index == next);
            ZEXPECT(argument.count >= 1);
            next = argument.index + argument.count;
        }
        ZEXPECT(next == argv.size());
    }
};

ZEST_CASE(locations_point_at_their_paths) {
    for(auto command: commands) {
        const auto argv = split(command);
        auto analysis = analyze_compiler(argv[0], argv, "", kotatsu::fs::read_file);
        ZASSERT(analysis.has_value());

        const auto names = [&](const Location& location, std::string_view path) {
            const auto& argument = analysis->arguments[location.argument];
            return location.index >= argument.index &&
                   location.index < argument.index + argument.count &&
                   argv[location.index].ends_with(path);
        };
        for(const auto& read: analysis->reads) {
            ZEXPECT(names(read.location, read.path));
        }
        for(const auto& diagnostic: analysis->diagnostics) {
            if(diagnostic.location.has_value()) {
                ZEXPECT(names(*diagnostic.location, *diagnostic.path));
            }
        }
    }
};

ZEST_CASE(writes_are_made_from_reads_in_argv_order) {
    for(auto command: commands) {
        const auto argv = split(command);
        auto analysis = analyze_compiler(argv[0], argv, "", kotatsu::fs::read_file);
        ZASSERT(analysis.has_value());

        ZEXPECT(std::ranges::is_sorted(analysis->reads, {}, [](const Read& read) {
            return read.location.index;
        }));
        for(const auto& write: analysis->writes) {
            for(auto read: write.reads) {
                ZEXPECT(read < analysis->reads.size());
            }
        }
    }
};

ZEST_CASE(missing_option_value_fails_the_parse) {
    const auto argv = split("gcc -c a.c -o");
    auto analysis = analyze_compiler(argv[0], argv, "", kotatsu::fs::read_file);
    ZASSERT(!analysis.has_value());
    ZEXPECT(analysis.error().kind == Error::Kind::CompilerParse);
};

ZEST_CASE(options_only_gcc_knows_take_their_values) {
    auto reads = [](std::string_view command) {
        const auto argv = split(command);
        auto analysis = analyze_compiler(argv[0], argv, "", kotatsu::fs::read_file);
        return analysis->reads | std::views::transform(&Read::path) |
               std::ranges::to<std::vector<std::string>>();
    };
    ZEXPECT(reads("gcc -c foo.c -dumpbase xx.c") == std::vector<std::string>{"foo.c"});
    ZEXPECT(reads("g++ -wrapper echo,foo.c -c bar.c") == std::vector<std::string>{"bar.c"});
    ZEXPECT(reads("gcc -aux-info protos.c -dumpbase-ext .c -c foo.c") ==
            std::vector<std::string>{"foo.c"});
    // cl's options of C++ modules, with either prefix, even where `/TP` makes any input a source.
    for(auto option: {"ifcMap", "ifcOutput", "ifcSearchDir", "scanDependencies", "stdIfcDir"}) {
        for(auto prefix: {"/", "-"}) {
            ZEST_CONTEXT("option: {}{}", prefix, option);
            ZEXPECT(reads(std::format("cl /c /TP a.cc {}{} value", prefix, option)) ==
                    std::vector<std::string>{"a.cc"});
        }
    }
};

ZEST_CASE(an_option_the_table_does_not_know_keeps_its_value) {
    const auto argv = split("gcc -c foo.c -dumpbase xx.c");
    auto analysis = analyze_compiler(argv[0], argv, "", kotatsu::fs::read_file);
    ZASSERT(analysis.has_value());
    const auto& argument = analysis->arguments.back();
    ZEXPECT(option_name(syntax_of(Compiler::GCC, Dialect::GCC), argument.id) == "<unknown>");
    ZEXPECT(argument.spelling == "-dumpbase");
    ZEXPECT(argument.values == std::vector<std::string>{"xx.c"});
};

ZEST_CASE(sources_are_read_in_the_language_their_selector_or_suffix_names) {
    auto languages = [](std::string_view command) {
        const auto argv = split(command);
        auto analysis = analyze_compiler(argv[0], argv, "", kotatsu::fs::read_file);
        std::vector<std::string> languages;
        for(const auto& read: analysis->reads) {
            languages.push_back(read.path + ":" + read.language.value_or("none"));
        }
        return languages;
    };
    ZEXPECT(languages("gcc -c a.c -x c++ b.cc -x none c.c d.cpp") ==
            std::vector<std::string>{"a.c:c", "b.cc:c++", "c.c:c", "d.cpp:c++"});
    ZEXPECT(languages("cl /c a.c /TC b.c") == std::vector<std::string>{"a.c:c", "b.c:c"});
    // `/TP` is for every input but objects and libraries, before it too.
    ZEXPECT(languages("cl a.c /TP b.cc c.obj noext /ifcOutput d.ifc") ==
            std::vector<std::string>{"a.c:c++", "b.cc:c++", "c.obj:none", "noext:c++"});
    // cl takes what follows `/` for an option, clang-cl for a file when its table knows none.
    ZEXPECT(languages("cl /c /TP a.cc /analyze /interface") ==
            std::vector<std::string>{"a.cc:c++"});
    ZEXPECT(languages("clang-cl /c /TP /src/a.cc /src/noext") ==
            std::vector<std::string>{"/src/a.cc:c++", "/src/noext:c++"});
    ZEXPECT(languages("g++ -x c++ -c legacy.c -x c -c noext -x c++-header pch.hxx.cxx") ==
            std::vector<std::string>{"legacy.c:c++", "noext:c", "pch.hxx.cxx:c++-header"});
    ZEXPECT(languages("clang -c a.cppm b.ixx c.m d.mm e.M f.s g.S h.sx i.cu") ==
            std::vector<std::string>{"a.cppm:c++-module",
                                     "b.ixx:c++-module",
                                     "c.m:objective-c",
                                     "d.mm:objective-c++",
                                     "e.M:objective-c++",
                                     "f.s:assembler",
                                     "g.S:assembler-with-cpp",
                                     "h.sx:assembler-with-cpp",
                                     "i.cu:cuda"});
    // cl takes no case: `.C` is C.
    ZEXPECT(languages("cl /c a.C b.CPP") == std::vector<std::string>{"a.C:c", "b.CPP:c++"});
    ZEXPECT(languages("cl /c /Tc a.cc /Tp b.c") == std::vector<std::string>{"a.cc:c", "b.c:c++"});
    ZEXPECT(languages("gcc a.o -o app") == std::vector<std::string>{"a.o:none"});
    ZEXPECT(languages("g++ -c a.C") == std::vector<std::string>{"a.C:c++"});
    ZEXPECT(languages("g++ -c a.c b.i -x c c.c") ==
            std::vector<std::string>{"a.c:c++", "b.i:c++", "c.c:c"});
    ZEXPECT(languages("clang++-18 -c a.c") == std::vector<std::string>{"a.c:c++"});
    ZEXPECT(languages("clang --driver-mode=g++ -c a.c") == std::vector<std::string>{"a.c:c++"});
    ZEXPECT(languages("clang++ --driver-mode=gcc -c a.c") == std::vector<std::string>{"a.c:c"});
    ZEXPECT(languages("x86_64-linux-gnu-c++ -c a.c") == std::vector<std::string>{"a.c:c++"});
    ZEXPECT(languages("cc -c a.c") == std::vector<std::string>{"a.c:c"});
};

};  // ZEST_SUITE(cmd_compiler_tests)
