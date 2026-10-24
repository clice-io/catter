#include "builtin/text.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace catter::builtin {

namespace {

/// The code point the UTF-8 sequence at `text[i]` encodes, and its length; U+FFFD and 1 for a
/// byte that starts no valid sequence.
struct Decoded {
    char32_t code_point;

    std::size_t length;
};

Decoded decode(std::string_view text, std::size_t i) {
    constexpr Decoded invalid{.code_point = 0xFFFD, .length = 1};
    const auto byte = [&](std::size_t at) {
        return static_cast<std::uint8_t>(text[at]);
    };
    const auto lead = byte(i);
    if(lead < 0x80) {
        return {.code_point = lead, .length = 1};
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
        return invalid;
    }
    if(i + length > text.size()) {
        return invalid;
    }
    for(std::size_t k = 1; k < length; k += 1) {
        if((byte(i + k) & 0xC0) != 0x80) {
            return invalid;
        }
        code_point = (code_point << 6) | (byte(i + k) & 0x3F);
    }
    if(code_point < least || code_point > 0x10FFFF ||
       (code_point >= 0xD800 && code_point <= 0xDFFF)) {
        return invalid;
    }
    return {.code_point = code_point, .length = length};
}

}  // namespace

std::u16string to_utf16(std::string_view text) {
    std::u16string out;
    out.reserve(text.size());
    for(std::size_t i = 0; i < text.size();) {
        const auto decoded = decode(text, i);
        i += decoded.length;
        if(decoded.code_point < 0x10000) {
            out.push_back(static_cast<char16_t>(decoded.code_point));
        } else {
            const auto rest = decoded.code_point - 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (rest >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (rest & 0x3FF)));
        }
    }
    return out;
}

std::string to_utf8(std::u16string_view text) {
    std::string out;
    out.reserve(text.size());
    for(std::size_t i = 0; i < text.size(); i += 1) {
        char32_t code_point = text[i];
        if(code_point >= 0xD800 && code_point < 0xDC00 && i + 1 < text.size() &&
           text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000) {
            code_point = 0x10000 + ((code_point - 0xD800) << 10) + (text[i + 1] - 0xDC00);
            i += 1;
        }
        if(code_point < 0x80) {
            out.push_back(static_cast<char>(code_point));
        } else if(code_point < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        } else if(code_point < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        }
    }
    return out;
}

std::string printable(std::string_view text) {
    return to_utf8(to_utf16(text));
}

std::string quote(std::u16string_view text) {
    std::u16string out = u"\"";
    for(std::size_t i = 0; i < text.size(); i += 1) {
        const auto unit = text[i];
        switch(unit) {
            case u'"': out += uR"(\")"; continue;
            case u'\\': out += uR"(\\)"; continue;
            case u'\b': out += uR"(\b)"; continue;
            case u'\f': out += uR"(\f)"; continue;
            case u'\n': out += uR"(\n)"; continue;
            case u'\r': out += uR"(\r)"; continue;
            case u'\t': out += uR"(\t)"; continue;
            default: break;
        }
        const bool high = unit >= 0xD800 && unit < 0xDC00;
        const bool low = unit >= 0xDC00 && unit < 0xE000;
        const bool paired =
            high && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000;
        if(unit < 0x20 || low || (high && !paired)) {
            for(auto c: std::format(R"(\u{:04x})", static_cast<unsigned>(unit))) {
                out.push_back(static_cast<char16_t>(c));
            }
            continue;
        }
        out.push_back(unit);
        if(paired) {
            i += 1;
            out.push_back(text[i]);
        }
    }
    out += u'"';
    return to_utf8(out);
}

}  // namespace catter::builtin
