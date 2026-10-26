module;

#include "modules/prelude.h"

module catter;

import :protocol.analysis;

using namespace catter;

namespace {

std::string wrong(std::vector<protocol::Pattern> filter) {
    return check_filter(filter).value_or("none");
}

using names = std::optional<std::vector<std::string>>;

}  // namespace

ZEST_SUITE(analysis_tests) {

ZEST_CASE(a_filter_names_kinds_names_and_phases_catter_knows) {
    ZEXPECT(wrong({
                {.kind = "compiler", .names = names({"gcc", "msvc"}), .phases = names({"link"})},
                {.kind = "tool", .names = names({"lld-coff"})},
                {.kind = "archiver"}
    }) == "none");
    ZEXPECT(
        wrong({{.kind = "linker"}}) ==
        "`linker` is no kind of program catter analyzes: give `compiler`, `archiver` or `tool`");
    ZEXPECT(wrong({
                {.kind = "compiler", .names = names({"icc"})}
    }) ==
            "`icc` is no compiler catter knows");
    ZEXPECT(wrong({
                {.kind = "compiler", .phases = names({"assemble"})}
    }) ==
            "`assemble` is no phase of a compiler");
    ZEXPECT(wrong({
                {.kind = "tool", .names = names({"ld"})}
    }) == "`ld` is no tool catter knows");
    ZEXPECT(wrong({
                {.kind = "tool", .phases = names({"link"})}
    }) ==
            "a pattern of tools names no phases");
    ZEXPECT(wrong({
                {.kind = "archiver", .names = names({"ar"})}
    }) ==
            "a pattern of archivers names no programs and no phases");
};

ZEST_CASE(a_filter_names_commands_by_their_analysis) {
    auto matches = [](std::vector<std::string> argv, std::vector<protocol::Pattern> filter) {
        return CommandAnalysis(argv[0], argv, "").matches(filter);
    };
    const std::vector<protocol::Pattern> archivers{{.kind = "archiver"}};
    const std::vector<protocol::Pattern> coff{
        {.kind = "tool", .names = names({"lld-coff"})}
    };
    const std::vector<protocol::Pattern> compiles{
        {.kind = "compiler", .phases = names({"compile"})}
    };
    ZEXPECT(matches({"ar", "rcs", "a.a", "a.o"}, archivers));
    ZEXPECT(!matches({"gcc", "-c", "a.c"}, archivers));
    ZEXPECT(matches({"lld-link", "/out:a.exe", "a.obj"}, coff));
    ZEXPECT(!matches({"ld.lld", "-o", "a", "a.o"}, coff));
    ZEXPECT(matches({"gcc", "-c", "a.c"}, compiles));
    ZEXPECT(!matches({"gcc", "a.o", "-o", "app"}, compiles));
    // A command whose arguments do not read has a program, but no phase.
    ZEXPECT(!matches({"gcc", "-c", "a.c", "-o"}, compiles));
    ZEXPECT(matches(
        {
            "gcc",
            "-c",
            "a.c",
            "-o"
    },
        {{.kind = "compiler", .names = names({"gcc"})}}));
    ZEXPECT(!matches({"make"}, {{.kind = "compiler"}, {.kind = "tool"}, {.kind = "archiver"}}));
};

ZEST_CASE(an_edit_of_an_unknown_kind_is_an_error) {
    std::vector<std::string> argv{"gcc", "-c", "a.c"};
    auto edited = CommandAnalysis(argv[0], argv, "")
                      .edit(argv, std::vector<protocol::CommandEdit>{{.kind = "replace"}});
    ZASSERT(!edited.has_value());
    ZEXPECT(edited.error() == "`replace` is no kind of edit: give `remove`, `append` or `prepend`");
};

};  // ZEST_SUITE(analysis_tests)
