module;

#include "support/messages.h"
#include "support/prelude.h"

module catter;

import catter.command;
import :feature.descendants;
import :feature.flat_tree;
import :feature.targets;
import :session.handler;

namespace catter::feature {

namespace {

/// A file a command writes and the files it writes it from, absolute.
struct Edge {
    std::string output;

    std::vector<std::string> inputs;
};

void add(std::vector<Edge>& edges,
         std::string_view cwd,
         std::string_view output,
         const auto& inputs) {
    if(output == "-") {
        return;
    }
    Edge edge;
    fs::path::resolve(edge.output, output, cwd);
    for(std::string_view input: inputs) {
        if(input != "-") {
            fs::path::resolve(edge.inputs.emplace_back(), input, cwd);
        }
    }
    edges.push_back(std::move(edge));
}

/// The files the command `argv` of `executable`, run in `cwd`, writes from those it reads, when
/// it is a compiler's, its response files read in, or an archiver's; the standard streams, `-`,
/// left out.
std::optional<std::vector<Edge>> edges_of(std::string_view cwd,
                                          std::string_view executable,
                                          std::span<const std::string> argv) {
    std::vector<Edge> edges;
    if(auto compiler = analyze_compiler(executable, argv, cwd, fs::read_file)) {
        for(const auto& write: compiler->writes) {
            add(edges, cwd, write.path, write.reads | views::transform([&](std::uint32_t read) {
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

/// The name a target shows by: its file's name, or, for a path that ends in none, the path.
std::string name_of(std::string_view path) {
    auto name = fs::path::filename(path);
    return kotatsu::replace_invalid_utf8(name.empty() ? path : name);
}

/// Renders the files a build makes as the forest of the files each is made from.
class TargetTree final : public Handler {
public:
    explicit TargetTree(TargetsOptions options) :
        max_depth(max_depth_of(options.depth.as_optional())) {}

    async::task<Action> on_command(data::ipcid_t id,
                                   std::optional<data::ipcid_t> parent,
                                   data::command command) override {
        descendants.add(id, parent);
        if(descendants.left_out(id)) {
            co_return Action{};
        }
        auto edges = edges_of(command.cwd, command.executable, command.args);
        if(!edges.has_value()) {
            co_return Action{};
        }
        for(auto& edge: *edges) {
            tree.merge(edge.output, {});
            for(auto& input: edge.inputs) {
                tree.merge(std::move(input), {}, edge.output);
            }
        }
        descendants.leave_out_descendants_of(id);
        co_return Action{};
    }

    async::task<> on_error(data::ipcid_t id,
                           [[maybe_unused]] std::optional<data::ipcid_t> parent,
                           [[maybe_unused]] std::string message) override {
        descendants.add(id, std::nullopt);
        co_return;
    }

    async::task<> on_finish(data::process_result result) override {
        print(tree,
              [](const Tree::Node& node) { return name_of(node.id); },
              max_depth,
              result.code,
              {.nodes = "targets", .node = "target", .tree = "target forest"});
        co_return;
    }

private:
    using Tree = FlatTree<std::string, std::monostate>;

    std::optional<std::uint32_t> max_depth;

    Descendants descendants;

    Tree tree;
};

}  // namespace

std::unique_ptr<Handler> make_targets(TargetsOptions options) {
    return std::make_unique<TargetTree>(std::move(options));
}

}  // namespace catter::feature
