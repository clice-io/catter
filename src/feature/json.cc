#include "feature/json.h"

#include <cassert>
#include <string>
#include <utility>
#include <kota/codec/json/json.h>

namespace catter::feature {

namespace {

/// A command's arguments need not be UTF-8, which JSON is.
struct Config {
    constexpr static auto invalid_utf8 = kota::codec::invalid_utf8::Replace;
};

}  // namespace

std::string to_json(const JSON& value) {
    auto compact = kota::codec::json::to_string<Config>(value);
    assert(compact.has_value());
    auto pretty = kota::codec::json::prettify(*compact);
    assert(pretty.has_value());
    return std::move(*pretty);
}

}  // namespace catter::feature
