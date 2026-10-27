module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter;

import :feature.flat_tree;
import :feature.json;
import :feature.tree;
import :session.handler;

namespace catter::feature {

namespace {

/// `argument` cut in its middle to `width` code points, `....` standing for what is cut, and
/// quoted when it holds a blank, a quote or a backslash.
std::string format_argument(std::string_view argument, std::int64_t width) {
    if(argument.empty()) {
        return R"("")";
    }
    auto text = kota::replace_invalid_utf8(argument);
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
class CmdTree final : public Handler {
public:
    explicit CmdTree(TreeOptions options) : options(std::move(options)) {}

    kota::task<Action> on_command(data::ipcid_t id,
                                  std::optional<data::ipcid_t> parent,
                                  data::command command) override {
        tree.merge(id, std::move(command.args), parent);
        co_return Action{};
    }

    kota::task<> on_error(data::ipcid_t id,
                          std::optional<data::ipcid_t> parent,
                          std::string message) override {
        tree.merge(id, std::unexpected(std::move(message)), parent);
        co_return;
    }

    kota::task<> on_finish(data::process_result result) override {
        print(
            tree,
            [&](const Tree::Node& node) { return text_of(node.content); },
            max_depth_of(options.depth.as_optional()),
            result.code,
            {.nodes = "commands", .node = "command", .tree = "command tree"});
        co_return;
    }

private:
    /// A command's argv, or why catter could not capture it.
    using Capture = std::expected<std::vector<std::string>, std::string>;

    using Tree = FlatTree<data::ipcid_t, Capture>;

    std::string text_of(const Capture& capture) const {
        if(!capture.has_value()) {
            return std::format("[capture error] {}", kota::replace_invalid_utf8(capture.error()));
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
    TreeOptions options;

    Tree tree;
};

}  // namespace

std::unique_ptr<Handler> make_tree(TreeOptions options) {
    return std::make_unique<CmdTree>(std::move(options));
}

}  // namespace catter::feature
