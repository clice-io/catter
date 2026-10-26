module;

#include "modules/prelude.h"

export module catter.command:error;

export namespace catter {

/// Why a command could not be analyzed as the kind of command asked for.
struct Error {
    enum class Kind : std::uint8_t {
        /// The executable is no compiler catter parses the arguments of.
        CompilerUnsupported,
        /// The compiler's arguments do not parse.
        CompilerParse,
        /// The executable is no archiver.
        ArchiverNotRecognized,
        /// An archiver run in a way catter does not model.
        ArchiverUnsupported,
        /// The executable is no tool catter reads the arguments of.
        ToolNotRecognized,
        /// The tool's arguments do not parse.
        ToolParse,
    };

    Kind kind;

    std::string message;
};

/// The name of `kind` in catter's output: "compiler-unsupported", ...
std::string_view name(Error::Kind kind);

}  // namespace catter
