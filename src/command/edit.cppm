module;

#include "modules/prelude.h"

export module catter.command:edit;

import :compiler;
import :response_file;
import :syntax;

export namespace catter {

/// One edit of a command's arguments: flags to remove from it, or to add to it, written as
/// the command's program takes them.
struct CommandEdit {
    enum class Kind : std::uint8_t {
        /// Takes out the arguments the flags name, by option and values: `-DX` those of `-D`
        /// with the value `X`, `-D*` or `-D` last every one of `-D`, `-W*` every option in
        /// `-W`'s group whose name starts with `-W`. An option the table does not know is named
        /// as written, `-fx=*` for any value.
        Remove,
        /// Adds the flags after the command's own arguments, where they take effect over them
        /// and libraries go: before `--` and `/link`, which end the options.
        Append,
        /// Adds the flags right after the program.
        Prepend,
    };

    Kind kind;

    std::vector<std::string> flags;
};

/// `argv`, argv[0] included, with `edits` made in turn to its arguments, which `syntax` reads
/// as `arguments` from `expansion`, argv with its response files read in; or what is wrong with
/// the edits: a remove takes out what an edit before it added too. The arguments no edit takes
/// out keep their elements as they are, and the flags an edit adds are added as they are
/// written. A response file stays as argv names it unless an edit takes out an argument it holds
/// or adds flags among them, which puts the arguments it holds in its place.
std::expected<std::vector<std::string>, std::string>
    apply_edits(std::span<const std::string> argv,
                const Expansion& expansion,
                std::span<const Argument> arguments,
                Syntax syntax,
                std::span<const CommandEdit> edits);

/// The same of a command whose response files are not read, whose arguments are read from
/// `argv` itself: a tool's.
std::expected<std::vector<std::string>, std::string>
    apply_edits(std::span<const std::string> argv,
                std::span<const Argument> arguments,
                Syntax syntax,
                std::span<const CommandEdit> edits);

}  // namespace catter
