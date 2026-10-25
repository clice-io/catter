#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <kota/deco/deco.h>

#include "handler.h"
#include "builtin/cli.h"
#include "builtin/flat_tree.h"
#include "builtin/handlers.h"
#include "builtin/text.h"
#include "util/data.h"

namespace catter::builtin {

namespace {

struct Options {
    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-d", "--depth", "--depth="},
                 meta_var = "<n>",
                 help = "Limit render depth.")
    <std::int64_t> depth;

    DecoKVStyled(
        kota::deco::decl::KVStyle::JoinedOrSeparate,
        names = {"-a", "--args", "--args="},
        meta_var = "<n>",
        help = "Show the first n arguments after argv[0]. Use -1 to keep all. [default: -1]")
    <std::int64_t> args = -1;

    DecoKVStyled(
        kota::deco::decl::KVStyle::JoinedOrSeparate,
        names = {"-w", "--argWidth", "--argWidth="},
        meta_var = "<n>",
        help = "Cut each longer argument in its middle to this many characters. [default: 10]")
    <std::int64_t> arg_width = 10;

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "cmd-tree",
    .description = "Render captured commands as a command tree.",
};

/// `argument` cut in its middle to `width` code points, `....` standing for what is cut, and
/// quoted when it holds a blank, a quote or a backslash.
std::string format_argument(std::string_view argument, std::int64_t width) {
    if(argument.empty()) {
        return R"("")";
    }
    auto text = printable(argument);
    const bool quoted = text.find_first_of(" \t\n\v\f\r\"'\\") != std::string::npos;
    std::vector<std::size_t> starts;
    for(std::size_t i = 0; i < text.size(); i += 1) {
        if((static_cast<std::uint8_t>(text[i]) & 0xC0) != 0x80) {
            starts.push_back(i);
        }
    }
    starts.push_back(text.size());
    const auto length = starts.size() - 1;
    if(std::cmp_greater(length, width)) {
        const auto kept = static_cast<std::size_t>(width - 4);
        text = text.substr(0, starts[(kept + 1) / 2]) + "...." +
               text.substr(starts[length - kept / 2]);
    }
    return quoted ? quote(text) : text;
}

/// Renders the commands of a build as the tree of the commands that started them.
class CmdTree final : public core::Handler {
public:
    explicit CmdTree(std::vector<std::string> args) : args(std::move(args)) {}

    kota::task<std::optional<core::Build>> on_start(core::Build build) override {
        auto parsed = parse_options<Options>(help, args);
        if(!parsed.has_value()) {
            co_return std::nullopt;
        }
        check_at_least<Options>(help, "--depth", parsed->depth.as_optional(), 0);
        check_at_least<Options>(help, "--args", *parsed->args, -1);
        check_at_least<Options>(help, "--argWidth", *parsed->arg_width, 4);
        options = std::move(*parsed);
        co_return build;
    }

    kota::task<core::Action> on_command(data::ipcid_t id,
                                        std::optional<data::ipcid_t> parent,
                                        data::command command) override {
        tree.merge(id, std::move(command.args), parent);
        co_return core::Action{};
    }

    kota::task<> on_error(data::ipcid_t id,
                          [[maybe_unused]] std::optional<data::ipcid_t> parent,
                          std::string message) override {
        tree.merge(id, std::unexpected(std::move(message)));
        co_return;
    }

    kota::task<> on_exit(data::ipcid_t, data::process_result) override {
        co_return;
    }

    kota::task<> on_finish(data::process_result result) override {
        if(result.code != 0) {
            std::println("Build failed with exit code {}. Printing partial command tree.",
                         result.code);
        }
        if(tree.size() == 0) {
            std::println("No commands found.");
            co_return;
        }
        const auto cycles = tree.assemble();
        const auto text = [&](const Tree::Node& node) {
            return text_of(node.content);
        };
        std::print("{}", render(tree, text, max_depth_of(options.depth.as_optional())));
        if(!cycles.empty()) {
            std::println("");
            std::println("Detected command cycles:");
            for(const auto& cycle: cycles) {
                const auto names = cycle | std::views::transform([&](data::ipcid_t id) {
                                       return text_of(tree.find(id)->content);
                                   }) |
                                   std::ranges::to<std::vector>();
                std::println("[cycle] {} -> {}",
                             names | std::views::join_with(std::string_view(" -> ")) |
                                 std::ranges::to<std::string>(),
                             names.front());
            }
        }
        co_return;
    }

private:
    /// A command's argv, or why catter could not capture it.
    using Capture = std::expected<std::vector<std::string>, std::string>;

    using Tree = FlatTree<data::ipcid_t, Capture>;

    std::string text_of(const Capture& capture) const {
        if(!capture.has_value()) {
            return std::format("[capture error] {}", printable(capture.error()));
        }
        const auto& argv = *capture;
        if(argv.empty()) {
            return "<empty command>";
        }
        const auto visible = *options.args;
        const auto shown = visible < 0
                               ? argv.size() - 1
                               : std::min(argv.size() - 1, static_cast<std::size_t>(visible));
        auto line = argv | std::views::take(shown + 1) |
                    std::views::transform([&](const std::string& argument) {
                        return format_argument(argument, *options.arg_width);
                    }) |
                    std::views::join_with(' ') | std::ranges::to<std::string>();
        if(shown + 1 < argv.size()) {
            line += " ...";
        }
        return line;
    }

private:
    std::vector<std::string> args;

    Options options;

    Tree tree;
};

}  // namespace

std::unique_ptr<core::Handler> make_cmd_tree(std::vector<std::string> args) {
    return std::make_unique<CmdTree>(std::move(args));
}

}  // namespace catter::builtin
