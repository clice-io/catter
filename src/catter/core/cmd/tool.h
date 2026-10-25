#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cmd/compiler.h"
#include "cmd/error.h"
#include "cmd/syntax.h"

namespace catter::cmd {

/// The programs catter reads the arguments of by their option tables, without analyzing the
/// files they read and write: linkers and librarians.
enum class Tool : std::uint8_t {
    LLDELF,
    LLDCOFF,
    LLDMachO,
    LLDMinGW,
    LLDWasm,
    LLVMLib,
    LLVMDlltool,
};

/// What catter makes of a command that runs a tool.
struct ToolAnalysis {
    Tool tool;

    /// Every argument after argv[0].
    std::vector<Argument> arguments;
};

/// The tool `executable` is, by its name; none for a program that is no tool catter knows. A
/// plain `ld` is the system's linker: GNU's on Linux, ld64 on macOS, MinGW's on Windows.
std::optional<Tool> identify_tool(std::string_view executable);

/// Reads the arguments of the command that runs `executable` with `argv`, argv[0] included.
std::expected<ToolAnalysis, Error> analyze_tool(std::string_view executable,
                                                std::span<const std::string> argv);

Syntax syntax_of(Tool tool);

/// The name of `tool` in catter's output, its option table's: "lld-elf", "llvm-lib", ...
std::string_view name(Tool tool);

}  // namespace catter::cmd
