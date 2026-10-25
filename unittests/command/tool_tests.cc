#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "command/tool.h"

using namespace catter;

namespace {

std::vector<std::string> split(std::string_view command) {
    return command | std::views::split(' ') | std::ranges::to<std::vector<std::string>>();
}

}  // namespace

ZEST_SUITE(cmd_tool_tests) {

ZEST_CASE(tools_are_identified_by_name) {
    auto tool_of = [](std::string_view command) {
        const auto argv = split(command);
        auto analysis = analyze_tool(argv[0], argv);
        return analysis.has_value() ? std::string(name(analysis->tool)) : "none";
    };
    EXPECT(tool_of("ld.lld -o app a.o") == "lld-elf");
    EXPECT(tool_of("/usr/bin/aarch64-linux-gnu-ld.bfd -o app a.o") == "lld-elf");
    EXPECT(tool_of("x86_64-w64-mingw32-ld -o app.exe a.o") == "lld-mingw");
    EXPECT(tool_of("ld64.lld -o app a.o") == "lld-macho");
    EXPECT(tool_of("lld-link /out:app.exe a.obj") == "lld-coff");
    EXPECT(tool_of("wasm-ld-18 -o app.wasm a.o") == "lld-wasm");
    EXPECT(tool_of("llvm-lib /out:a.lib a.obj") == "llvm-lib");
    EXPECT(tool_of("llvm-dlltool -d a.def -l a.lib") == "llvm-dlltool");
    EXPECT(tool_of("gcc -c a.c") == "none");
    // A plain `ld` is the system's linker.
#if defined(CATTER_WINDOWS)
    EXPECT(tool_of("ld -o app.exe a.o") == "lld-mingw");
#elif defined(CATTER_MAC)
    EXPECT(tool_of("ld -o app a.o") == "lld-macho");
#else
    EXPECT(tool_of("ld -o app a.o") == "lld-elf");
#endif
};

ZEST_CASE(a_tool_s_arguments_are_read_by_its_table) {
    const auto argv = split("ld.lld --gc-sections -o app a.o -lm");
    auto analysis = analyze_tool(argv[0], argv);
    ASSERT(analysis.has_value());
    const auto syntax = syntax_of(analysis->tool);
    std::vector<std::string> options;
    for(const auto& argument: analysis->arguments) {
        options.emplace_back(option_name(syntax, argument.id));
    }
    EXPECT(options == std::vector<std::string>{"--gc-sections", "-o", "<input>", "-l"});
};

};  // ZEST_SUITE(cmd_tool_tests)
