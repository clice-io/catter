#include "builtin/text.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace catter::builtin {

namespace {

/// The length of the valid UTF-8 sequence at `text[i]`; 0 for a byte that starts none.
std::size_t sequence_length(std::string_view text, std::size_t i) {
    const auto byte = [&](std::size_t at) {
        return static_cast<std::uint8_t>(text[at]);
    };
    const auto lead = byte(i);
    if(lead < 0x80) {
        return 1;
    }
    std::size_t length;
    char32_t code_point;
    char32_t least;
    if((lead & 0xE0) == 0xC0) {
        length = 2;
        code_point = lead & 0x1F;
        least = 0x80;
    } else if((lead & 0xF0) == 0xE0) {
        length = 3;
        code_point = lead & 0x0F;
        least = 0x800;
    } else if((lead & 0xF8) == 0xF0) {
        length = 4;
        code_point = lead & 0x07;
        least = 0x10000;
    } else {
        return 0;
    }
    if(i + length > text.size()) {
        return 0;
    }
    for(std::size_t k = 1; k < length; k += 1) {
        if((byte(i + k) & 0xC0) != 0x80) {
            return 0;
        }
        code_point = (code_point << 6) | (byte(i + k) & 0x3F);
    }
    if(code_point < least || code_point > 0x10FFFF ||
       (code_point >= 0xD800 && code_point <= 0xDFFF)) {
        return 0;
    }
    return length;
}

}  // namespace

std::string printable(std::string_view text) {
    constexpr std::string_view replacement_character = "\xEF\xBF\xBD";
    std::string out;
    out.reserve(text.size());
    for(std::size_t i = 0; i < text.size();) {
        if(const auto length = sequence_length(text, i); length != 0) {
            out.append(text.substr(i, length));
            i += length;
        } else {
            out.append(replacement_character);
            i += 1;
        }
    }
    return out;
}

std::string quote(std::string_view text) {
    std::string out = R"(")";
    for(const char c: printable(text)) {
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

}  // namespace catter::builtin
