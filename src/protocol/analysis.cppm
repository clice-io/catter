module;

#include "modules/prelude.h"

module catter:protocol.analysis;

import catter.command;
import :protocol.protocol;

namespace catter {

/// What catter makes of a command, for the handlers that ask: the analysis of a compiler's or
/// an archiver's command, the arguments of a tool's, or why the arguments of one of them do
/// not read.
class CommandAnalysis {
public:
    /// The analysis of the command that runs `executable` with `argv` in `cwd`, whose response
    /// files are read from there.
    CommandAnalysis(std::string_view executable,
                    std::span<const std::string> argv,
                    std::string_view cwd);

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

        Error error;
    };

    std::variant<std::monostate, CompilerAnalysis, ArchiverAnalysis, ToolAnalysis, Unparsed> result;
};

/// What is wrong with `filter`, a handler's; none when catter takes it.
std::optional<std::string> check_filter(std::span<const protocol::Pattern> filter);

}  // namespace catter
