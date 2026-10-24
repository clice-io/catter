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

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-w", "--argWidth", "--argWidth="},
                 meta_var = "<n>",
                 help = "Limit the display width of each argument. [default: 10]")
    <std::int64_t> arg_width = 10;

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "cmd-tree",
    .description = "Render captured commands as a command tree.",
};

/// Whether `text` holds what JavaScript's `\s` matches, a quote or a backslash.
bool needs_quotes(std::u16string_view text) {
    return std::ranges::any_of(text, [](char16_t unit) {
        switch(unit) {
            case u'\t':
            case u'\n':
            case u'\v':
            case u'\f':
            case u'\r':
            case u' ':
            case 0x00A0:
            case 0x1680:
            case 0x2028:
            case 0x2029:
            case 0x202F:
            case 0x205F:
            case 0x3000:
            case 0xFEFF:
            case u'"':
            case u'\'':
            case u'\\': return true;
            default: return unit >= 0x2000 && unit <= 0x200A;
        }
    });
}

/// `argument` cut in its middle to `width` code units, `....` standing for what is cut, and
/// quoted when it holds a space, a quote or a backslash.
std::string format_argument(std::string_view argument, std::int64_t width) {
    auto text = to_utf16(argument);
    if(text.empty()) {
        return R"("")";
    }
    const bool quoted = needs_quotes(text);
    if(std::cmp_greater(text.size(), width)) {
        const auto kept = static_cast<std::size_t>(width - 4);
        text = text.substr(0, (kept + 1) / 2) + u"...." + text.substr(text.size() - kept / 2);
    }
    return quoted ? quote(text) : to_utf8(text);
}

/// Renders the commands of a build as the tree of the commands that started them.
class CmdTree final : public core::Handler {
public:
    explicit CmdTree(std::vector<std::string> args) : args(std::move(args)) {}

    kota::task<std::optional<core::Build>> on_start(core::Build build) override {
        auto options = parse_options<Options>(help, args);
        if(!options.has_value()) {
            co_return std::nullopt;
        }
        check_at_least<Options>(help, "--depth", options->depth.as_optional(), 0);
        check_at_least<Options>(help, "--args", *options->args, -1);
        check_at_least<Options>(help, "--argWidth", *options->arg_width, 4);
        if(options->depth.has_value()) {
            max_depth = static_cast<std::uint32_t>(*options->depth);
        }
        visible_args = *options->args;
        arg_width = *options->arg_width;
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
        std::print("{}", render(tree, text, max_depth));
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
        const auto shown = visible_args < 0
                               ? argv.size() - 1
                               : std::min(argv.size() - 1, static_cast<std::size_t>(visible_args));
        auto line = argv | std::views::take(shown + 1) |
                    std::views::transform([&](const std::string& argument) {
                        return format_argument(argument, arg_width);
                    }) |
                    std::views::join_with(' ') | std::ranges::to<std::string>();
        if(shown + 1 < argv.size()) {
            line += " ...";
        }
        return line;
    }

    std::vector<std::string> args;

    std::optional<std::uint32_t> max_depth;

    std::int64_t visible_args = -1;

    std::int64_t arg_width = 10;

    Tree tree;
};

}  // namespace

std::unique_ptr<core::Handler> make_cmd_tree(std::vector<std::string> args) {
    return std::make_unique<CmdTree>(std::move(args));
}

}  // namespace catter::builtin
