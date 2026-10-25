#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <kota/deco/option.h>

#include "command/compiler.h"

namespace catter {

/// How a program's arguments are read: by an option table, and those of its options the
/// program takes.
struct Syntax {
    const kota::option::OptTable* table;

    std::uint32_t visibility;
};

/// The syntax of a compiler's arguments.
Syntax syntax_of(Dialect dialect);

/// The arguments of `argv`, argv[0] included, as `syntax` reads them, or why they do not parse.
std::expected<std::vector<Argument>, std::string> read_arguments(Syntax syntax,
                                                                 std::span<const std::string> argv);

/// The name of the option `id` with its prefix: `-I`, `/Fo`; `<input>` and `<unknown>` of an
/// input and of an option the table does not know.
std::string_view option_name(Syntax syntax, std::uint32_t id);

}  // namespace catter
