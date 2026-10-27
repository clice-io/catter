module;

#include "modules/prelude.h"

module catter;

import :feature.json;

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

std::string quote(std::string_view text) {
    std::string out = R"(")";
    for(const char c: kota::replace_invalid_utf8(text)) {
        switch(c) {
            case '"': out += R"(\")"; break;
            case '\\': out += R"(\\)"; break;
            case '\b': out += R"(\b)"; break;
            case '\f': out += R"(\f)"; break;
            case '\n': out += R"(\n)"; break;
            case '\r': out += R"(\r)"; break;
            case '\t': out += R"(\t)"; break;
            default:
                if(static_cast<std::uint8_t>(c) < 0x20) {
                    out += std::format(R"(\u{:04x})", static_cast<unsigned>(c));
                } else {
                    out += c;
                }
        }
    }
    out += '"';
    return out;
}

}  // namespace catter::feature
