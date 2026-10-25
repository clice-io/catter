#include "feature/edges.h"

#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "command/archiver.h"
#include "command/compiler.h"
#include "feature/path.h"

namespace catter::feature {

namespace {

void add(std::vector<Edge>& edges,
         std::string_view cwd,
         std::string_view output,
         const auto& inputs) {
    if(output == "-") {
        return;
    }
    Edge edge{.output = absolute_path(cwd, output)};
    for(std::string_view input: inputs) {
        if(input != "-") {
            edge.inputs.push_back(absolute_path(cwd, input));
        }
    }
    edges.push_back(std::move(edge));
}

}  // namespace

std::optional<std::vector<Edge>> edges_of(std::string_view cwd,
                                          std::string_view executable,
                                          std::span<const std::string> argv) {
    std::vector<Edge> edges;
    if(auto compiler = analyze_compiler(executable, argv)) {
        for(const auto& write: compiler->writes) {
            add(edges,
                cwd,
                write.path,
                write.reads | std::views::transform([&](std::uint32_t read) {
                    return std::string_view(compiler->reads[read].path);
                }));
        }
        return edges;
    }
    if(auto archiver = analyze_archiver(executable, argv)) {
        for(const auto& write: archiver->writes) {
            add(edges, cwd, write, archiver->reads);
        }
        return edges;
    }
    return std::nullopt;
}

}  // namespace catter::feature
