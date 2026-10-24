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
#include <kota/deco/deco.h>

#include "handler.h"
#include "builtin/cli.h"
#include "builtin/descendants.h"
#include "builtin/edges.h"
#include "builtin/flat_tree.h"
#include "builtin/handlers.h"
#include "builtin/text.h"
#include "util/data.h"
#include "util/encoding.h"

namespace catter::builtin {

namespace {

struct Options {
    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-d", "--depth", "--depth="},
                 meta_var = "<n>",
                 help = "Limit render depth.")
    <std::int64_t> depth;

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "target-tree",
    .description = "Render captured build targets as a flat target forest.",
};

/// The name a target shows by: its file's name, or, for a path that ends in none, the path.
std::string name_of(std::string_view path) {
    auto name = util::to_utf8(util::from_utf8(path).filename());
    return printable(name.empty() ? path : name);
}

/// Renders the files a build makes as the forest of the files each is made from.
class TargetTree final : public core::Handler {
public:
    explicit TargetTree(std::vector<std::string> args) : args(std::move(args)) {}

    kota::task<std::optional<core::Build>> on_start(core::Build build) override {
        auto options = parse_options<Options>(help, args);
        if(!options.has_value()) {
            co_return std::nullopt;
        }
        check_at_least<Options>(help, "--depth", options->depth.as_optional(), 0);
        max_depth = max_depth_of(options->depth.as_optional());
        co_return build;
    }

    kota::task<core::Action> on_command(data::ipcid_t id,
                                        std::optional<data::ipcid_t> parent,
                                        data::command command) override {
        descendants.add(id, parent);
        if(descendants.left_out(id)) {
            co_return core::Action{};
        }
        auto edges = edges_of(command.cwd, command.executable, command.args);
        if(!edges.has_value()) {
            co_return core::Action{};
        }
        for(auto& edge: *edges) {
            tree.merge(edge.output, {});
            for(auto& input: edge.inputs) {
                tree.merge(std::move(input), {}, edge.output);
            }
        }
        descendants.leave_out_descendants_of(id);
        co_return core::Action{};
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

    std::vector<std::string> args;

    std::optional<std::uint32_t> max_depth;

    Descendants descendants;

    Tree tree;
};

}  // namespace

std::unique_ptr<core::Handler> make_target_tree(std::vector<std::string> args) {
    return std::make_unique<TargetTree>(std::move(args));
}

}  // namespace catter::builtin
