module;

#include "support/prelude.h"

#include "support/messages.h"

module catter;

import :feature.json;

namespace catter::feature {

namespace {

/// A command's arguments need not be UTF-8, which JSON is.
struct Config {
    constexpr static auto invalid_utf8 = codec::invalid_utf8::Replace;
};

/// `argument` as a command line shows it: as it is when plain, else as a JSON string.
std::string quote_argument(std::string_view argument) {
    const auto plain = !argument.empty() && ranges::all_of(argument, [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               std::string_view(R"(_@%+=:,./\-)").contains(c);
    });
    return plain ? std::string(argument) : quote(argument);
}

}  // namespace

std::string to_json(const JSON& value, Layout layout) {
    auto compact = json::to_string<Config>(value);
    assert(compact.has_value());
    if(!layout.pretty) {
        return std::move(*compact);
    }
    auto pretty = json::prettify(*compact);
    assert(pretty.has_value());
    return std::move(*pretty);
}

std::string quote(std::string_view text) {
    std::string out = R"(")";
    for(const char c: kotatsu::replace_invalid_utf8(text)) {
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

std::string command_line(const data::command& command) {
    if(command.args.empty()) {
        return quote_argument(command.executable);
    }
    return command.args | views::transform(quote_argument) | views::join_with(' ') |
           ranges::to<std::string>();
}

void save_file(std::string_view file, std::string_view text, std::string_view what) {
    auto created = fs::create_directories(fs::path::parent(file));
    auto error = created.has_value() ? fs::write_file_atomic(file, text) : created.error();
    if(error) {
        throw cpptrace::runtime_error(
            std::format("Failed to write the {} `{}`: {}", what, file, error.message()));
    }
}

}  // namespace catter::feature
