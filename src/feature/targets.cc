#include "feature/targets.h"

#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include <kota/support/utf8.h>
#include <kota/fs/path.h>

#include "feature/descendants.h"
#include "feature/edges.h"
#include "feature/flat_tree.h"
#include "session/data.h"
#include "session/handler.h"

namespace catter::feature {

namespace {

/// The name a target shows by: its file's name, or, for a path that ends in none, the path.
std::string name_of(std::string_view path) {
    auto name = kota::fs::path::filename(path);
    return kota::replace_invalid_utf8(name.empty() ? path : name);
}

/// Renders the files a build makes as the forest of the files each is made from.
class TargetTree final : public Handler {
public:
    explicit TargetTree(TargetsOptions options) :
        max_depth(max_depth_of(options.depth.as_optional())) {}

    kota::task<std::optional<Build>> on_start(Build build) override {
        co_return build;
    }

    kota::task<Action> on_command(data::ipcid_t id,
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

    kota::task<> on_error(data::ipcid_t id,
                          [[maybe_unused]] std::optional<data::ipcid_t> parent,
                          [[maybe_unused]] std::string message) override {
        descendants.add(id, std::nullopt);
        co_return;
    }

    kota::task<> on_exit(data::ipcid_t, data::process_result) override {
        co_return;
    }

    kota::task<> on_finish(data::process_result result) override {
        if(result.code != 0) {
            std::println("Build failed with exit code {}. Printing partial target forest.",
                         result.code);
        }
        if(tree.size() == 0) {
            std::println("No targets found.");
            co_return;
        }
        const auto cycles = tree.assemble();
        std::print(
            "{}",
            render(tree, [](const Tree::Node& node) { return name_of(node.id); }, max_depth));
        if(!cycles.empty()) {
            std::println("");
            std::println("Detected target cycles:");
            for(const auto& cycle: cycles) {
                const auto names =
                    cycle | std::views::transform(name_of) | std::ranges::to<std::vector>();
                std::println("[cycle] {} -> {}",
                             names | std::views::join_with(std::string_view(" -> ")) |
                                 std::ranges::to<std::string>(),
                             names.front());
            }
        }
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
