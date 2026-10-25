#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "cmd/compiler.h"
#include "cmd/syntax.h"

namespace catter::cmd {

/// One edit of a command's arguments: flags to remove from it, or to add to it, written as
/// the command's program takes them.
struct CommandEdit {
    enum class Kind : std::uint8_t {
        /// Takes out the arguments the flags name, by option and values: `-DX` the definition
        /// of X, `-D*` or `-D` last every definition, `-W*` every option of `-W`'s group whose
        /// spelling starts with `-W`. An option the table does not know is named as written,
        /// `-fx=*` for any value.
        Remove,
        /// Adds the flags before the first input, where they take effect for every input:
        /// before `--` and `/link`, which end the options.
        Append,
        /// Adds the flags right after the program.
        Prepend,
    };

    Kind kind;

    std::vector<std::string> flags;
};

/// `argv`, argv[0] included, whose arguments `syntax` reads as `arguments`, with `edits` made
/// in turn, or what is wrong with them: a remove takes out what an edit before it added too.
/// The arguments no edit takes out keep their elements of argv as they are, and the flags an
/// edit adds are added as they are written.
std::expected<std::vector<std::string>, std::string>
    apply_edits(std::span<const std::string> argv,
                std::span<const Argument> arguments,
                Syntax syntax,
                std::span<const CommandEdit> edits);

}  // namespace catter::cmd
