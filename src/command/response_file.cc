module;

#include "modules/prelude.h"

module catter.command;

namespace catter {

namespace {

/// How deep response files are read in a response file: past it, `@file` stays as it is, which
/// ends a file that names itself.
constexpr std::uint32_t max_depth = 8;

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::vector<std::string> split_gnu(std::string_view text) {
    std::vector<std::string> arguments;
    std::optional<std::string> argument;
    for(std::size_t i = 0; i < text.size(); i += 1) {
        const char c = text[i];
        if(is_space(c)) {
            if(argument.has_value()) {
                arguments.push_back(std::move(*argument));
                argument.reset();
            }
            continue;
        }
        auto& token = argument.has_value() ? *argument : argument.emplace();
        if(c == '\\' && i + 1 < text.size()) {
            i += 1;
            token += text[i];
        } else if(c == '\'' || c == '"') {
            for(i += 1; i < text.size() && text[i] != c; i += 1) {
                if(text[i] == '\\' && i + 1 < text.size()) {
                    i += 1;
                }
                token += text[i];
            }
        } else {
            token += c;
        }
    }
    if(argument.has_value()) {
        arguments.push_back(std::move(*argument));
    }
    return arguments;
}

/// Adds to `token` the run of backslashes at `i`, and the double quote after it when they take
/// it as it is; the index of the last character added.
std::size_t add_backslashes(std::string_view text, std::size_t i, std::string& token) {
    const auto end = std::min(text.find_first_not_of('\\', i), text.size());
    const auto count = end - i;
    if(end == text.size() || text[end] != '"') {
        token.append(count, '\\');
        return end - 1;
    }
    token.append(count / 2, '\\');
    if(count % 2 == 0) {
        return end - 1;
    }
    token += '"';
    return end;
}

std::vector<std::string> split_windows(std::string_view text) {
    std::vector<std::string> arguments;
    std::optional<std::string> argument;
    bool quoted = false;
    for(std::size_t i = 0; i < text.size(); i += 1) {
        const char c = text[i];
        if(!quoted && is_space(c)) {
            if(argument.has_value()) {
                arguments.push_back(std::move(*argument));
                argument.reset();
            }
            continue;
        }
        auto& token = argument.has_value() ? *argument : argument.emplace();
        if(c == '\\') {
            i = add_backslashes(text, i, token);
        } else if(c != '"') {
            token += c;
        } else if(quoted && i + 1 < text.size() && text[i + 1] == '"') {
            token += '"';
            i += 1;
        } else {
            quoted = !quoted;
        }
    }
    if(argument.has_value()) {
        arguments.push_back(std::move(*argument));
    }
    return arguments;
}

/// The text of a response file's `bytes`: those after a UTF-8 byte order mark, or UTF-16's in
/// UTF-8, as MSVC's tools write them.
std::string text_of(std::string bytes) {
    if(bytes.starts_with("\xFF\xFE")) {
        std::u16string units;
        for(std::size_t i = 2; i + 1 < bytes.size(); i += 2) {
            units += static_cast<char16_t>(static_cast<unsigned char>(bytes[i]) |
                                           static_cast<unsigned char>(bytes[i + 1]) << 8);
        }
        std::string text;
        kota::from_utf16(text, units);
        return text;
    }
    if(bytes.starts_with("\xEF\xBB\xBF")) {
        bytes.erase(0, 3);
    }
    return bytes;
}

struct Builder {
    std::string_view directory;

    Quoting quoting;

    ReadFile read;

    Expansion expansion;

    /// Adds `argument`, which the element `origin` of the command's argv is or names the file
    /// of, read `depth` response files deep.
    void add(std::string_view argument, std::uint32_t origin, std::uint32_t depth) {
        if(argument.starts_with('@') && depth < max_depth) {
            if(auto text = read_file(argument.substr(1))) {
                for(const auto& each:
                    quoting == Quoting::GNU ? split_gnu(*text) : split_windows(*text)) {
                    add(each, origin, depth + 1);
                }
                return;
            }
        }
        expansion.argv.emplace_back(argument);
        expansion.origins.push_back(origin);
    }

    std::optional<std::string> read_file(std::string_view name) {
        std::string path(directory);
        kota::fs::path::push(path, name);
        std::string bytes;
        if(read(bytes, path)) {
            return std::nullopt;
        }
        expansion.files.emplace_back(name);
        return text_of(std::move(bytes));
    }
};

}  // namespace

Expansion expand_response_files(std::span<const std::string> argv,
                                std::string_view directory,
                                Quoting quoting,
                                ReadFile read) {
    Builder builder{.directory = directory, .quoting = quoting, .read = read};
    for(std::uint32_t i = 0; i < argv.size(); i += 1) {
        builder.add(argv[i], i, 0);
    }
    return std::move(builder.expansion);
}

}  // namespace catter
