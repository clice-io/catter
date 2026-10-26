module;

#include "modules/prelude.h"

#include "utils/response_files.h"

module catter;

import catter.command;

using namespace catter;

namespace {

using Strings = std::vector<std::string>;

/// The arguments of the response file `text`, split as `quoting` says.
Strings split(std::string_view text, Quoting quoting) {
    ResponseFiles files{.files = {{"/work/a.rsp", std::string(text)}}};
    const Strings argv = {"cc", "@a.rsp"};
    auto expansion = expand_response_files(argv, "/work", quoting, files);
    return Strings(expansion.argv.begin() + 1, expansion.argv.end());
}

/// The UTF-16LE bytes of the ASCII `text`.
std::string utf16(std::string_view text) {
    std::string bytes;
    for(char c: text) {
        bytes += c;
        bytes += '\0';
    }
    return bytes;
}

}  // namespace

ZEST_SUITE(cmd_response_file_tests) {

ZEST_CASE(gnu_quoting_takes_quotes_of_either_kind_and_escapes) {
    ZEXPECT(split("-DA\n\t-DB=\"two words\" 'single \"quoted\"'  back\\ slash", Quoting::GNU) ==
            Strings{"-DA", "-DB=two words", R"(single "quoted")", "back slash"});
    ZEXPECT(split(R"(-DC=\"q\" "a\"b" '' C:\src\x.c)", Quoting::GNU) ==
            Strings{R"(-DC="q")", R"(a"b)", "", "C:srcx.c"});
};

ZEST_CASE(windows_quoting_takes_double_quotes_and_backslashes_before_them) {
    ZEXPECT(split(R"(/c /Fo"out dir\\" /DX=\"1\" "a b.cc" C:\src\x.cc "" 'q')", Quoting::Windows) ==
            Strings{"/c", R"(/Foout dir\)", R"(/DX="1")", "a b.cc", R"(C:\src\x.cc)", "", "'q'"});
    ZEXPECT(split(R"("a""b" a""b \\\\"x y" end\)", Quoting::Windows) ==
            Strings{R"(a"b)", "ab", R"(\\x y)", R"(end\)"});
};

ZEST_CASE(files_nest_and_name_files_from_the_directory) {
    // `@d.rsp` in sub/b.rsp names d.rsp of the directory, not sub/d.rsp.
    ResponseFiles files{
        .files = {
                  {"/work/a.rsp", "-DA @sub/b.rsp @/abs/c.rsp"},
                  {"/work/sub/b.rsp", "-DB @d.rsp"},
                  {"/work/d.rsp", "-DD"},
                  {"/work/sub/d.rsp", "-DWRONG"},
                  {"/abs/c.rsp", "-DC"},
                  }
    };
    const Strings argv = {"gcc", "@a.rsp", "-c", "x.c"};
    auto expansion = expand_response_files(argv, "/work", Quoting::GNU, files);
    ZEXPECT(expansion.argv == Strings{"gcc", "-DA", "-DB", "-DD", "-DC", "-c", "x.c"});
    ZEXPECT(expansion.origins == std::vector<std::uint32_t>{0, 1, 1, 1, 1, 2, 3});
    ZEXPECT(expansion.files == Strings{"a.rsp", "sub/b.rsp", "d.rsp", "/abs/c.rsp"});
};

ZEST_CASE(a_file_that_cannot_be_read_stays_as_named) {
    ResponseFiles files{.files = {{"/work/a.rsp", "-DA @gone.rsp"}}};
    const Strings argv = {"gcc", "@missing.rsp", "@a.rsp", "-c", "x.c"};
    auto expansion = expand_response_files(argv, "/work", Quoting::GNU, files);
    ZEXPECT(expansion.argv == Strings{"gcc", "@missing.rsp", "-DA", "@gone.rsp", "-c", "x.c"});
    ZEXPECT(expansion.origins == std::vector<std::uint32_t>{0, 1, 2, 2, 3, 4});
    ZEXPECT(expansion.files == Strings{"a.rsp"});
};

ZEST_CASE(a_file_that_names_itself_ends) {
    ResponseFiles files{.files = {{"/work/loop.rsp", "-DX @loop.rsp"}}};
    const Strings argv = {"gcc", "@loop.rsp"};
    auto expansion = expand_response_files(argv, "/work", Quoting::GNU, files);
    ZEXPECT(expansion.files.size() == 8);
    ZEXPECT(std::ranges::count(expansion.argv, "-DX") == 8);
    ZEXPECT(expansion.argv.back() == "@loop.rsp");
};

ZEST_CASE(a_byte_order_mark_says_the_encoding) {
    ZEXPECT(split("\xEF\xBB\xBF-DX", Quoting::GNU) == Strings{"-DX"});
    ZEXPECT(split("\xFF\xFE" + utf16("/c \"a b.cc\""), Quoting::Windows) ==
            Strings{"/c", "a b.cc"});
    // U+00E9, U+4E2D and U+1F600, the last a surrogate pair, then a surrogate without its pair.
    ZEXPECT(split(std::string("\xFF\xFE\xE9\x00\x2D\x4E\x3D\xD8\x00\xDE\x20\x00\x3D\xD8", 14),
                  Quoting::Windows) == Strings{"\u00E9\u4E2D\U0001F600", "\uFFFD"});
};

ZEST_CASE(compilers_split_their_files_as_their_driver_does) {
    ResponseFiles files{.files = {{"/work/a.rsp", R"(/c C:\src\a.cc)"}}};
    auto sources = [&](Strings argv) {
        auto analysis = analyze_compiler(argv[0], argv, "/work", files);
        Strings sources;
        for(const auto& read: analysis->reads) {
            sources.push_back(read.path);
        }
        return sources;
    };
    ZEXPECT(sources({"cl", "@a.rsp"}) == Strings{R"(C:\src\a.cc)"});
    ZEXPECT(sources({"clang", "--driver-mode=cl", "@a.rsp"}) == Strings{R"(C:\src\a.cc)"});
    ZEXPECT(sources({"clang-cl", "@a.rsp"}) == Strings{R"(C:\src\a.cc)"});
    ZEXPECT(sources({"clang", "-c", "@a.rsp"}) == Strings{"C:srca.cc"});
};

ZEST_CASE(a_compiler_reads_its_arguments_from_its_files) {
    ResponseFiles files{.files = {{"/work/flags.rsp", "-DA -c a.c -o"}}};
    const Strings argv = {"gcc", "@flags.rsp", "a.o", "@gone.rsp"};
    auto analysis = analyze_compiler(argv[0], argv, "/work", files);
    ZASSERT(analysis.has_value());
    ZEXPECT(analysis->expansion.argv ==
            Strings{"gcc", "-DA", "-c", "a.c", "-o", "a.o", "@gone.rsp"});
    ZEXPECT(analysis->expansion.files == Strings{"flags.rsp"});
    ZEXPECT(analysis->arguments.size() == 5);
    ZEXPECT(analysis->arguments[3].index == 4);
    ZEXPECT(analysis->arguments[3].values == Strings{"a.o"});
    ZASSERT(analysis->reads.size() == 1);
    ZEXPECT(analysis->reads[0].location.index == 3);
    ZASSERT(analysis->writes.size() == 1);
    ZEXPECT(analysis->writes[0].path == "a.o");
    ZASSERT(analysis->diagnostics.size() == 1);
    const auto& unread = analysis->diagnostics[0];
    ZEXPECT(unread.code == Diagnostic::Code::ResponseFileUnread);
    ZEXPECT(unread.path == "gone.rsp");
    ZEXPECT(unread.location->index == 6);
};

};  // ZEST_SUITE(cmd_response_file_tests)
