module;

#include "modules/prelude.h"

module catter.command;

namespace catter {

namespace {

/// The options of GCC's driver that take a separate value and clang's table does not know.
constexpr auto gcc_separate =
    std::to_array<std::string_view>({"-aux-info", "-dumpbase", "-dumpbase-ext", "-wrapper"});

/// Those of cl, its options of C++ modules, with either prefix.
constexpr auto cl_separate = std::to_array<std::string_view>({
    "/ifcMap",
    "-ifcMap",
    "/ifcOutput",
    "-ifcOutput",
    "/ifcSearchDir",
    "-ifcSearchDir",
    "/scanDependencies",
    "-scanDependencies",
    "/stdIfcDir",
    "-stdIfcDir",
});

}  // namespace

Syntax syntax_of(Compiler compiler, Dialect dialect) {
    using namespace option::clang;
    switch(dialect) {
        case Dialect::Clang: return {.table = &table(), .visibility = DefaultVis};
        case Dialect::GCC:
            return {.table = &table(), .visibility = DefaultVis, .separate = gcc_separate};
        case Dialect::MSVC:
            return {.table = &table(),
                    .visibility = DefaultVis | CLOption,
                    .separate = cl_separate,
                    .slash_options = compiler == Compiler::MSVC};
        // nvcc's arguments are not read yet.
        case Dialect::NVCC: break;
    }
    std::unreachable();
}

std::expected<std::vector<Argument>, std::string>
    read_arguments(Syntax syntax, std::span<const std::string> argv) {
    std::vector<Argument> arguments;
    // A replayed command may have no argv at all.
    auto start = std::min<std::size_t>(argv.size(), 1);
    while(start < argv.size()) {
        // The table reads again after an option of `syntax.separate` and its value.
        auto next = argv.size();
        auto rest = argv.subspan(start);
        for(const auto& result: syntax.table->parse(rest,
                                                    {
                                                        .dash_dash_parsing = true,
                                                        .visibility = syntax.visibility,
                                                    })) {
            if(!result.has_value()) {
                const auto index = start + result.error().index;
                return std::unexpected(
                    std::format("fatal error in parsing: failed to parse `{}` (arg #{}) : {}",
                                argv[index],
                                index,
                                result.error().message));
            }
            const auto index = static_cast<std::uint32_t>(start + result->index);
            if(std::ranges::contains(syntax.separate, argv[index])) {
                // An edit that removes it may name it alone.
                const auto values =
                    argv.subspan(index + 1, std::min<std::size_t>(argv.size() - index - 1, 1));
                arguments.push_back(Argument{
                    .id = syntax.table->unknown_option_id,
                    .index = index,
                    .count = static_cast<std::uint32_t>(1 + values.size()),
                    .spelling = argv[index],
                    .values = values | std::ranges::to<std::vector<std::string>>(),
                });
                next = index + 1 + values.size();
                break;
            }
            const bool option = syntax.slash_options &&
                                result->id == syntax.table->input_option_id &&
                                argv[index].starts_with('/');
            arguments.push_back(Argument{
                .id = option ? syntax.table->unknown_option_id : result->id,
                .index = index,
                .count = result->next_index - result->index,
                .spelling = result->spelling,
                .values = result->values | std::ranges::to<std::vector<std::string>>(),
            });
        }
        start = next;
    }
    return arguments;
}

std::string_view option_name(Syntax syntax, std::uint32_t id) {
    return syntax.table->option(id)->prefixed_name();
}

}  // namespace catter
