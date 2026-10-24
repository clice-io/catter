#include "cmd/archiver.h"

#include <algorithm>
#include <array>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cmd/lowercase.h"

namespace catter::cmd {

namespace {

/// The name of an executable without its directory and `.exe`, lowered.
std::string stem_of(std::string_view executable) {
    auto name = lowercase(executable.substr(executable.find_last_of("/\\") + 1));
    if(name.ends_with(".exe")) {
        name.resize(name.size() - 4);
    }
    return name;
}

/// Options before the operation that change no file: `--format=gnu`, `--plugin x`, `-X64`, ...
bool skips(std::string_view token) {
    constexpr auto bare = std::to_array<std::string_view>(
        {"-X32", "-X64", "-X32_64", "-Xany", "--version", "--help", "-h"});
    constexpr auto valued =
        std::to_array<std::string_view>({"--format", "--plugin", "--rsp-quoting", "--output"});
    return std::ranges::contains(bare, token) ||
           std::ranges::any_of(valued, [&](std::string_view option) {
               return token.starts_with(option) &&
                      (token.size() == option.size() || token[option.size()] == '=');
           });
}

bool takes_value(std::string_view token) {
    return token == "--format" || token == "--plugin" || token == "--rsp-quoting" ||
           token == "--output";
}

struct OperationToken {
    char operation;

    std::string modifiers;
};

/// The operation letter of `token` and the modifier letters around it.
std::optional<OperationToken> parse_operation(std::string_view token) {
    if(token.starts_with('-')) {
        token.remove_prefix(1);
    }
    if(token == "s") {
        return OperationToken{.operation = 's'};
    }
    std::optional<char> operation;
    std::string modifiers;
    for(char letter: token) {
        if(std::string_view("dmpqrtx").contains(letter)) {
            if(operation.has_value()) {
                return std::nullopt;
            }
            operation = letter;
        } else {
            modifiers.push_back(letter);
        }
    }
    if(!operation.has_value()) {
        return std::nullopt;
    }
    return OperationToken{.operation = *operation, .modifiers = std::move(modifiers)};
}

Error unsupported(std::string message) {
    return Error{.kind = Error::Kind::ArchiverUnsupported, .message = std::move(message)};
}

}  // namespace

std::expected<ArchiverAnalysis, Error> analyze_archiver(std::string_view executable,
                                                        std::span<const std::string> argv) {
    const auto stem = stem_of(executable);
    if(stem != "ar" && stem != "llvm-ar" && stem != "gcc-ar") {
        return std::unexpected(Error{
            .kind = Error::Kind::ArchiverNotRecognized,
            .message = std::format("not a recognized archiver executable: {}", executable),
        });
    }

    std::size_t index = 1;
    bool thin = false;
    for(; index < argv.size(); index += 1) {
        const std::string_view token = argv[index];
        if(token == "-M") {
            return std::unexpected(unsupported("archiver MRI script mode is not supported"));
        }
        if(token == "--thin" || token == "-T") {
            thin = true;
        } else if(skips(token)) {
            if(takes_value(token) && index + 1 < argv.size()) {
                index += 1;
            }
        } else if(token.starts_with("--") || token.starts_with('@')) {
            return std::unexpected(
                unsupported(std::format("unsupported archiver option: {}", token)));
        } else {
            break;
        }
    }

    if(index >= argv.size()) {
        return std::unexpected(unsupported("archiver command has no operation token"));
    }
    auto operation = parse_operation(argv[index]);
    if(!operation.has_value()) {
        return std::unexpected(
            unsupported(std::format("unsupported archiver operation token: {}", argv[index])));
    }
    index += 1;
    if(index >= argv.size()) {
        return std::unexpected(unsupported("archiver command has no archive path"));
    }
    auto archive = argv[index];
    index += 1;

    ArchiverAnalysis analysis{
        .modifiers = std::move(operation->modifiers),
        .thin = thin,
        .archive = archive,
        .members = std::vector<std::string>(argv.begin() + index, argv.end()),
    };
    switch(operation->operation) {
        case 'p': analysis.operation = Operation::Print; break;
        case 'q': analysis.operation = Operation::QuickAppend; break;
        case 'r': analysis.operation = Operation::ReplaceOrInsert; break;
        case 't': analysis.operation = Operation::Table; break;
        default:
            return std::unexpected(unsupported(
                std::format("archiver operation is not modeled: {}", operation->operation)));
    }
    if(analysis.operation == Operation::Print || analysis.operation == Operation::Table) {
        analysis.reads = {std::move(archive)};
    } else {
        analysis.reads = analysis.members;
        analysis.writes = {std::move(archive)};
    }
    return analysis;
}

std::string_view name(Operation operation) {
    switch(operation) {
        case Operation::Print: return "p";
        case Operation::QuickAppend: return "q";
        case Operation::ReplaceOrInsert: return "r";
        case Operation::Table: return "t";
    }
    std::unreachable();
}

}  // namespace catter::cmd
