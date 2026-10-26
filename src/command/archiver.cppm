module;

#include "modules/prelude.h"

export module catter.command:archiver;

import :error;

export namespace catter {

/// The operations of `ar` catter models the files of.
enum class Operation : std::uint8_t {
    /// `p`: prints members.
    Print,
    /// `q`: appends members.
    QuickAppend,
    /// `r`: replaces or inserts members.
    ReplaceOrInsert,
    /// `t`: lists members.
    Table,
};

/// What catter makes of a command that runs `ar`, `llvm-ar` or `gcc-ar`.
struct ArchiverAnalysis {
    Operation operation;

    /// The letters of the operation's argument but the operation, `cs` of `rcs`.
    std::string modifiers;

    /// `--thin` or `-T`.
    bool thin;

    std::string archive;

    std::vector<std::string> members;

    /// The archive, read by `p` and `t`, or the members, read by `q` and `r`.
    std::vector<std::string> reads;

    /// The archive, written by `q` and `r`, from every read.
    std::vector<std::string> writes;
};

/// Analyzes the command that runs `executable` with `argv`, argv[0] included.
std::expected<ArchiverAnalysis, Error> analyze_archiver(std::string_view executable,
                                                        std::span<const std::string> argv);

/// The letter of `operation`: "p", "q", "r" or "t".
std::string_view name(Operation operation);

}  // namespace catter
