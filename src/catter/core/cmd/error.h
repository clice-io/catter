#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace catter::cmd {

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
    };

    Kind kind;

    std::string message;
};

/// The name of `kind` in catter's output: "compiler-unsupported", ...
std::string_view name(Error::Kind kind);

}  // namespace catter::cmd
