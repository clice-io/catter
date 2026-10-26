module;

#include "modules/prelude.h"

module catter.command;

import :executable;

namespace catter {

namespace {

struct Rule {
    Tool tool;

    std::regex executable;
};

Rule rule_of(Tool tool, std::string_view name) {
    return {.tool = tool, .executable = executable_pattern(name, version_suffix)};
}

/// In match order.
const auto& rules() {
    const static std::array rules = {
        rule_of(Tool::LLDMinGW, R"([^\\/]*mingw32[^\\/]*-ld(?:\.(?:bfd|lld))?)"),
        rule_of(Tool::LLDELF, R"((?:[^\\/]+-)?ld\.(?:lld|bfd|gold|mold)|mold)"),
        rule_of(Tool::LLDMachO, R"(ld64(?:\.lld)?)"),
        rule_of(Tool::LLDCOFF, "lld-link"),
        rule_of(Tool::LLDWasm, R"((?:[^\\/]+-)?wasm-ld)"),
        rule_of(Tool::LLVMLib, "llvm-lib"),
        rule_of(Tool::LLVMDlltool, R"((?:[^\\/]+-)?(?:llvm-)?dlltool)"),
#if defined(CATTER_WINDOWS)
        rule_of(Tool::LLDMinGW, R"((?:[^\\/]+-)?ld)"),
        rule_of(Tool::LLDCOFF, "link"),
        rule_of(Tool::LLVMLib, "lib"),
#elif defined(CATTER_MAC)
        rule_of(Tool::LLDMachO, "ld"),
#else
        rule_of(Tool::LLDELF, R"((?:[^\\/]+-)?ld)"),
#endif
    };
    return rules;
}

}  // namespace

std::optional<Tool> identify_tool(std::string_view executable) {
    for(const auto& rule: rules()) {
        if(std::regex_match(executable.begin(), executable.end(), rule.executable)) {
            return rule.tool;
        }
    }
    return std::nullopt;
}

std::expected<ToolAnalysis, Error> analyze_tool(std::string_view executable,
                                                std::span<const std::string> argv) {
    auto tool = identify_tool(executable);
    if(!tool.has_value()) {
        return std::unexpected(Error{
            .kind = Error::Kind::ToolNotRecognized,
            .message = std::format("not a recognized tool executable: {}", executable),
        });
    }
    auto arguments = read_arguments(syntax_of(*tool), argv);
    if(!arguments.has_value()) {
        return std::unexpected(Error{
            .kind = Error::Kind::ToolParse,
            .message = std::move(arguments.error()),
        });
    }
    return ToolAnalysis{.tool = *tool, .arguments = std::move(*arguments)};
}

Syntax syntax_of(Tool tool) {
    switch(tool) {
        case Tool::LLDELF: return {.table = &option::lld_elf::table(), .visibility = ~0U};
        case Tool::LLDCOFF: return {.table = &option::lld_coff::table(), .visibility = ~0U};
        case Tool::LLDMachO: return {.table = &option::lld_macho::table(), .visibility = ~0U};
        case Tool::LLDMinGW: return {.table = &option::lld_mingw::table(), .visibility = ~0U};
        case Tool::LLDWasm: return {.table = &option::lld_wasm::table(), .visibility = ~0U};
        case Tool::LLVMLib: return {.table = &option::llvm_lib::table(), .visibility = ~0U};
        case Tool::LLVMDlltool: return {.table = &option::llvm_dlltool::table(), .visibility = ~0U};
    }
    std::unreachable();
}

std::string_view name(Tool tool) {
    switch(tool) {
        case Tool::LLDELF: return "lld-elf";
        case Tool::LLDCOFF: return "lld-coff";
        case Tool::LLDMachO: return "lld-macho";
        case Tool::LLDMinGW: return "lld-mingw";
        case Tool::LLDWasm: return "lld-wasm";
        case Tool::LLVMLib: return "llvm-lib";
        case Tool::LLVMDlltool: return "llvm-dlltool";
    }
    std::unreachable();
}

}  // namespace catter
