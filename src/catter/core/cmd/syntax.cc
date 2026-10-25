#include "cmd/syntax.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <kota/deco/option.h>

#include "option/clang.h"

namespace catter::cmd {

Syntax syntax_of(Dialect dialect) {
    using namespace opt::clang;
    switch(dialect) {
        case Dialect::Clang:
        case Dialect::GCC: return {.table = &table(), .visibility = DefaultVis};
        case Dialect::MSVC: return {.table = &table(), .visibility = DefaultVis | CLOption};
        // nvcc's arguments are not read yet.
        case Dialect::NVCC: break;
    }
    std::unreachable();
}

std::expected<std::vector<Argument>, std::string>
    read_arguments(Syntax syntax, std::span<const std::string> argv) {
    // A replayed command may have no argv at all.
    auto args = argv.subspan(std::min<std::size_t>(argv.size(), 1));
    std::vector<Argument> arguments;
    for(const auto& result: syntax.table->parse(args,
                                                {
                                                    .dash_dash_parsing = true,
                                                    .visibility = syntax.visibility,
                                                })) {
        if(!result.has_value()) {
            const auto index = result.error().index + 1;
            return std::unexpected(
                std::format("fatal error in parsing: failed to parse `{}` (arg #{}) : {}",
                            argv[index],
                            index,
                            result.error().message));
        }
        arguments.push_back(Argument{
            .id = result->id,
            .index = result->index + 1,
            .count = result->next_index - result->index,
            .spelling = result->spelling,
            .values = result->values | std::ranges::to<std::vector<std::string>>(),
        });
    }
    return arguments;
}

std::string_view option_name(Syntax syntax, std::uint32_t id) {
    return syntax.table->option(id)->prefixed_name();
}

}  // namespace catter::cmd
