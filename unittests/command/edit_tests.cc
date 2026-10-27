module;

#include "modules/prelude.h"

#include "utils/response_files.h"

module catter;

import catter.command;

using namespace catter;

namespace {

std::vector<std::string> split(std::string_view command) {
    return command | std::views::split(' ') | std::ranges::to<std::vector<std::string>>();
}

std::string join(const std::vector<std::string>& argv) {
    std::string text;
    for(const auto& element: argv) {
        text += (text.empty() ? "" : " ") + element;
    }
    return text;
}

CommandEdit removing(std::string_view flags) {
    return {.kind = CommandEdit::Kind::Remove, .flags = split(flags)};
}

CommandEdit appending(std::string_view flags) {
    return {.kind = CommandEdit::Kind::Append, .flags = split(flags)};
}

CommandEdit prepending(std::string_view flags) {
    return {.kind = CommandEdit::Kind::Prepend, .flags = split(flags)};
}

/// `command`, whose response files are `files`, with `edits` made, or the error.
std::string edited(std::string_view command,
                   std::initializer_list<CommandEdit> edits,
                   const ResponseFiles& files = {}) {
    const auto argv = split(command);
    auto analysis = analyze_compiler(argv[0], argv, "", files);
    if(!analysis.has_value()) {
        return "analysis: " + analysis.error().message;
    }
    auto result = apply_edits(argv,
                              analysis->expansion,
                              analysis->arguments,
                              syntax_of(analysis->identity.compiler, analysis->dialect),
                              std::vector(edits));
    return result.has_value() ? join(*result) : "error: " + result.error();
}

}  // namespace

ZEST_SUITE(cmd_edit_tests) {

ZEST_CASE(arguments_no_edit_takes_out_keep_their_spelling) {
    ZEXPECT(edited("gcc -msmall-data-limit=8 -I include -c a.c", {removing("-DX")}) ==
            "gcc -msmall-data-limit=8 -I include -c a.c");
    ZEXPECT(edited("cl /nologo /W3 /J /c a.cc", {appending("/DX")}) ==
            "cl /nologo /W3 /J /c a.cc /DX");
};

ZEST_CASE(a_remove_names_options_by_their_values) {
    ZEXPECT(edited("gcc -DA -DB -DA=1 -c a.c", {removing("-DA")}) == "gcc -DB -DA=1 -c a.c");
    ZEXPECT(edited("gcc -DA -D B -c a.c", {removing("-D*")}) == "gcc -c a.c");
    ZEXPECT(edited("gcc -include x.h -c a.c -include y.h", {removing("-c -include")}) == "gcc a.c");
    ZEXPECT(edited("cl /DA /c a.cc", {removing("-DA")}) == "cl /c a.cc");
};

ZEST_CASE(a_remove_of_a_prefix_takes_the_options_in_its_group) {
    ZEXPECT(edited("gcc -Wall -Wextra -Wframe-larger-than=2048 -Wl,--as-needed -c a.c",
                   {removing("-W*")}) == "gcc -Wl,--as-needed -c a.c");
    ZEXPECT(edited("cl /W3 /c a.cc", {removing("-W*")}) == "cl /c a.cc");
    // `-o` is in no group: `-o*` takes `-o` with any value, and nothing else.
    ZEXPECT(edited("gcc -o a.o -object -c a.c", {removing("-o*")}) == "gcc -object -c a.c");
};

ZEST_CASE(a_remove_names_an_unknown_option_as_written) {
    ZEXPECT(edited("gcc -fno-such-option=3 -fno-such-flag -c a.c",
                   {removing("-fno-such-option=*")}) == "gcc -fno-such-flag -c a.c");
    ZEXPECT(edited("gcc -fno-such-flag -c a.c", {removing("-fno-such-flag")}) == "gcc -c a.c");
    // One the table does not know but takes a value goes with it.
    ZEXPECT(edited("gcc -c a.c -dumpbase b.c", {removing("-dumpbase")}) == "gcc -c a.c");
    ZEXPECT(edited("cl /c a.cc -ifcOutput a.ifc", {removing("-ifcOutput")}) == "cl /c a.cc");
};

ZEST_CASE(appended_flags_go_after_the_options_as_written) {
    ZEXPECT(edited("gcc -c a.c -std=c11", {appending("-I inc -std=c17")}) ==
            "gcc -c a.c -std=c11 -I inc -std=c17");
    ZEXPECT(edited("gcc a.o b.o -o app", {appending("-lm")}) == "gcc a.o b.o -o app -lm");
    ZEXPECT(edited("clang -c -- a.c", {appending("-DX")}) == "clang -c -DX -- a.c");
    ZEXPECT(edited("cl /nologo a.cc /link /dll", {appending("/DX")}) ==
            "cl /nologo a.cc /DX /link /dll");
};

ZEST_CASE(prepended_flags_follow_the_program) {
    ZEXPECT(edited("gcc -c a.c", {prepending("-Wall"), prepending("-DX")}) ==
            "gcc -Wall -DX -c a.c");
};

ZEST_CASE(clang_in_cl_mode_takes_cl_flags) {
    ZEXPECT(edited("clang --driver-mode=cl /c /DX a.c", {removing("/DX"), appending("/DY")}) ==
            "clang --driver-mode=cl /c a.c /DY");
};

ZEST_CASE(a_remove_takes_back_what_an_edit_before_it_added) {
    ZEXPECT(edited("gcc -DX -c a.c", {appending("-DX -DY"), removing("-DX")}) == "gcc -c a.c -DY");
    ZEXPECT(edited("gcc -c a.c", {prepending("-DX"), removing("-DX")}) == "gcc -c a.c");
    ZEXPECT(edited("gcc -c a.c", {removing("-DX"), appending("-DX")}) == "gcc -c a.c -DX");
};

ZEST_CASE(flags_that_name_no_option_are_errors) {
    ZEXPECT(edited("gcc -c a.c", {removing("a.c")}) == "error: `a.c` names no option to remove");
    ZEXPECT(edited("gcc -c a.c", {appending("-o")})
                .starts_with("error: the flags to add do not parse"));
};

ZEST_CASE(a_response_file_no_edit_changes_stays_as_named) {
    const ResponseFiles files{
        .files = {
                  {"flags.rsp", "-DX -DY"},
                  {"empty.rsp", ""},
                  {"cl.rsp", "/c /DX a.cc"},
                  }
    };
    ZEXPECT(edited("gcc @flags.rsp -c a.c", {appending("-DZ"), prepending("-O2")}, files) ==
            "gcc -O2 @flags.rsp -c a.c -DZ");
    ZEXPECT(edited("cl /nologo @cl.rsp", {appending("/DY")}, files) == "cl /nologo @cl.rsp /DY");
    ZEXPECT(edited("gcc @empty.rsp -DX -c a.c @empty.rsp", {removing("-DX")}, files) ==
            "gcc @empty.rsp -c a.c @empty.rsp");
    ZEXPECT(edited("gcc @gone.rsp -c a.c", {appending("-DZ")}, files) ==
            "gcc @gone.rsp -c a.c -DZ");
};

ZEST_CASE(an_edit_among_the_arguments_of_a_response_file_puts_them_in_its_place) {
    const ResponseFiles files{
        .files = {
                  {"flags.rsp", "-DX -DY @nested.rsp"},
                  {"nested.rsp", "-DN"},
                  {"all.rsp", "-c a.c -- b.c"},
                  {"output.rsp", "-c a.c -o"},
                  }
    };
    ZEXPECT(edited("gcc @flags.rsp -c a.c", {removing("-DX")}, files) == "gcc -DY -DN -c a.c");
    ZEXPECT(edited("clang @all.rsp", {appending("-DZ")}, files) == "clang -c a.c -DZ -- b.c");
    ZEXPECT(edited("gcc @output.rsp a.o -DX", {removing("-o")}, files) == "gcc -c a.c -DX");
};

};  // ZEST_SUITE(cmd_edit_tests)
