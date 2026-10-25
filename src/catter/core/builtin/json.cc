#include "builtin/json.h"

#include <cassert>
#include <string>
#include <utility>
#include <kota/codec/json/json.h>

#include "builtin/text.h"

namespace catter::builtin {

std::string to_json(const JSON& value) {
    auto compact = kota::codec::json::to_string(value);
    assert(compact.has_value());
    // The parser under prettify takes only UTF-8, which a command's arguments need not be.
    auto pretty = kota::codec::json::prettify(printable(*compact));
    assert(pretty.has_value());
    return std::move(*pretty);
}

}  // namespace catter::builtin
