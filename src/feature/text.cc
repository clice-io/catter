#include "feature/text.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <kota/support/utf8.h>

namespace catter::feature {

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
