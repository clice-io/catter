#pragma once

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "protocol.h"
#include "cmd/archiver.h"
#include "cmd/compiler.h"
#include "cmd/error.h"
#include "cmd/tool.h"

namespace catter::core {

/// What catter makes of a command, for the handlers that ask: the analysis of a compiler's or
/// an archiver's command, the arguments of a tool's, or why the arguments of one of them do
/// not read.
class CommandAnalysis {
public:
    CommandAnalysis(std::string_view executable, std::span<const std::string> argv);

    /// Whether a pattern of `filter` names the command.
    bool matches(std::span<const protocol::Pattern> filter) const;

    /// The analysis as the protocol sends it; none for a program catter knows nothing of.
    std::optional<protocol::Analysis> message() const;

    /// `argv`, the command's, with `edits` made, or why catter cannot make them.
    std::expected<std::vector<std::string>, std::string>
        edit(std::span<const std::string> argv, std::span<const protocol::CommandEdit> edits) const;

private:
    /// A program catter knows, whose arguments do not read.
    struct Unparsed {
        /// `compiler`, `archiver` or `tool`.
        std::string_view of;

        std::string name;

        cmd::Error error;
    };

    std::variant<std::monostate,
                 cmd::CompilerAnalysis,
                 cmd::ArchiverAnalysis,
                 cmd::ToolAnalysis,
                 Unparsed>
        result;
};

/// What is wrong with `filter`, a handler's; none when catter takes it.
std::optional<std::string> check_filter(std::span<const protocol::Pattern> filter);

}  // namespace catter::core
