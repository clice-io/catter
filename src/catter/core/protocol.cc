#include "protocol.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <simdjson.h>

namespace catter::protocol {

namespace {

/// U+FFFD in UTF-8.
constexpr std::string_view replacement_character = "\xEF\xBF\xBD";

constexpr std::string_view alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string encode_base64(std::string_view bytes) {
    std::string text;
    text.reserve((bytes.size() + 2) / 3 * 4);
    for(std::size_t i = 0; i < bytes.size(); i += 3) {
        std::uint32_t group = static_cast<std::uint8_t>(bytes[i]) << 16;
        if(i + 1 < bytes.size()) {
            group |= static_cast<std::uint8_t>(bytes[i + 1]) << 8;
        }
        if(i + 2 < bytes.size()) {
            group |= static_cast<std::uint8_t>(bytes[i + 2]);
        }
        text += alphabet[(group >> 18) & 0x3f];
        text += alphabet[(group >> 12) & 0x3f];
        text += i + 1 < bytes.size() ? alphabet[(group >> 6) & 0x3f] : '=';
        text += i + 2 < bytes.size() ? alphabet[group & 0x3f] : '=';
    }
    return text;
}

std::expected<std::string, std::string> decode_base64(std::string_view text) {
    if(text.size() % 4 != 0) {
        return std::unexpected(
            std::format("`{}` is not base64: its length is not a multiple of 4", text));
    }
    std::string bytes;
    bytes.reserve(text.size() / 4 * 3);
    for(std::size_t i = 0; i < text.size(); i += 4) {
        std::uint32_t group = 0;
        int padding = 0;
        for(std::size_t j = 0; j < 4; j += 1) {
            auto c = text[i + j];
            std::uint32_t value = 0;
            if(c == '=' && i + 4 == text.size() && j >= 2) {
                padding += 1;
            } else if(auto pos = alphabet.find(c); pos != std::string_view::npos && padding == 0) {
                value = static_cast<std::uint32_t>(pos);
            } else {
                return std::unexpected(std::format("`{}` is not base64", text));
            }
            group = (group << 6) | value;
        }
        bytes += static_cast<char>((group >> 16) & 0xff);
        if(padding < 2) {
            bytes += static_cast<char>((group >> 8) & 0xff);
        }
        if(padding < 1) {
            bytes += static_cast<char>(group & 0xff);
        }
    }
    return bytes;
}

}  // namespace

OSString to_os_string(std::string bytes) {
    if(simdjson::validate_utf8(bytes.data(), bytes.size())) {
        return bytes;
    }
    return Base64{.base64 = encode_base64(bytes)};
}

std::vector<OSString> to_os_string(std::vector<std::string> bytes) {
    std::vector<OSString> texts;
    texts.reserve(bytes.size());
    for(auto& item: bytes) {
        texts.push_back(to_os_string(std::move(item)));
    }
    return texts;
}

std::expected<std::string, std::string> to_bytes(OSString text) {
    if(auto* plain = std::get_if<std::string>(&text)) {
        return std::move(*plain);
    }
    return decode_base64(std::get<Base64>(text).base64);
}

std::expected<std::vector<std::string>, std::string> to_bytes(std::vector<OSString> texts) {
    std::vector<std::string> bytes;
    bytes.reserve(texts.size());
    for(auto& text: texts) {
        auto item = to_bytes(std::move(text));
        if(!item) {
            return std::unexpected(std::move(item.error()));
        }
        bytes.push_back(std::move(*item));
    }
    return bytes;
}

std::string to_text(std::string bytes) {
    if(simdjson::validate_utf8(bytes.data(), bytes.size())) {
        return bytes;
    }
    std::string text;
    std::size_t i = 0;
    while(i < bytes.size()) {
        // Bytes are valid UTF-8 alone only when they hold whole characters:
        // the shortest valid run from `i` is one character, and a byte that
        // starts none has no valid run.
        const auto longest = std::min<std::size_t>(4, bytes.size() - i);
        std::size_t length = 1;
        while(length <= longest && !simdjson::validate_utf8(bytes.data() + i, length)) {
            length += 1;
        }
        if(length > longest) {
            text += replacement_character;
            i += 1;
        } else {
            text.append(bytes, i, length);
            i += length;
        }
    }
    return text;
}

}  // namespace catter::protocol
