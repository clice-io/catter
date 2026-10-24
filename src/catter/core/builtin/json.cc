#include "builtin/json.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <variant>
#include <kota/codec/dyn/dyn.h>

#include "builtin/text.h"

namespace catter::builtin {

namespace {

namespace dyn = kota::codec::dyn;

void write_string(std::string& out, std::string_view text) {
    out += quote(to_utf16(text));
}

void write(std::string& out, const JSON& value, std::size_t depth);

/// The items of an array or the entries of an object, a line each, or `[]` and `{}` for none.
template <typename Items, typename Item>
void write_items(std::string& out,
                 const Items& items,
                 std::size_t depth,
                 std::string_view brackets,
                 Item&& write_item) {
    out += brackets[0];
    if(items.empty()) {
        out += brackets[1];
        return;
    }
    bool first = true;
    for(const auto& item: items) {
        out += first ? "\n" : ",\n";
        first = false;
        out.append(2 * (depth + 1), ' ');
        write_item(item);
    }
    out += '\n';
    out.append(2 * depth, ' ');
    out += brackets[1];
}

void write(std::string& out, const JSON& value, std::size_t depth) {
    std::visit(
        [&](const auto& v) {
            using T = std::remove_cvref_t<decltype(v)>;
            if constexpr(std::is_same_v<T, std::monostate>) {
                out += "null";
            } else if constexpr(std::is_same_v<T, bool>) {
                out += v ? "true" : "false";
            } else if constexpr(std::is_same_v<T, double>) {
                out += std::isfinite(v) ? std::format("{}", v) : "null";
            } else if constexpr(std::is_integral_v<T>) {
                out += std::format("{}", v);
            } else if constexpr(std::is_same_v<T, std::string>) {
                write_string(out, v);
            } else if constexpr(std::is_same_v<T, dyn::Array>) {
                write_items(out, v, depth, "[]", [&](const JSON& item) {
                    write(out, item, depth + 1);
                });
            } else {
                write_items(out, v, depth, "{}", [&](const dyn::Object::entry& entry) {
                    write_string(out, entry.first);
                    out += ": ";
                    write(out, entry.second, depth + 1);
                });
            }
        },
        value.variant());
}

}  // namespace

std::string to_json(const JSON& value) {
    std::string out;
    write(out, value, 0);
    return out;
}

}  // namespace catter::builtin
