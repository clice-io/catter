#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace catter::feature {

/// A file a command writes and the files it writes it from, absolute.
struct Edge {
    std::string output;

    std::vector<std::string> inputs;
};

/// The files the command `argv` of `executable`, run in `cwd`, writes from those it reads, when
/// it is a compiler's or an archiver's; the standard streams, `-`, left out.
std::optional<std::vector<Edge>> edges_of(std::string_view cwd,
                                          std::string_view executable,
                                          std::span<const std::string> argv);

}  // namespace catter::feature
