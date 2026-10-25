#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "command/compiler.h"

namespace catter {

/// A positional argument of a compiler: a file it reads, or not, as its suffix says.
struct Candidate {
    std::string path;

    Location location;

    /// The language `-x` set for the inputs after it, or `/TC` or `/TP` for every input, when
    /// one did.
    std::optional<std::string> language;
};

/// What an output option names.
enum class OutputKind : std::uint8_t {
    /// `-o`: whatever the command makes.
    PrimaryArtifact,
    /// `/Fo`: the objects of a compile.
    ObjectFile,
    /// `/Fe`, `/link /out:`: what a link makes.
    LinkedArtifact,
};

struct Output {
    std::string path;

    Location location;

    OutputKind kind;
};

/// What the arguments of a compiler command say, before they are resolved into files.
struct ParsedCommand {
    Dialect dialect;

    /// Whether it reads C sources as C++: its identity's, unless `--driver-mode` says otherwise.
    bool cxx = false;

    std::vector<Argument> arguments;

    /// The last triple `--target=` or `-target` gives.
    std::optional<std::string> triple;

    Phase phase;

    Artifact artifact;

    /// The files the syntax says are read: the values of `/Tc` and `/Tp`, the inputs after
    /// `/link`.
    std::vector<Read> inputs;

    std::vector<Candidate> candidates;

    std::vector<Output> outputs;

    /// Whether `/FA` or `/Fa` asks for an assembly listing.
    bool listing = false;

    /// The last path `/Fa` names the listing by.
    std::optional<std::string> listing_path;
};

/// Parses the arguments of a command that runs the compiler `identity` names, but nvcc.
std::expected<ParsedCommand, Error> parse(const Identity& identity,
                                          std::span<const std::string> argv);

std::string_view name(OutputKind kind);

}  // namespace catter
