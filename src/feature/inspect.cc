#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/deco/deco.h>
#include <kota/codec/dyn/dyn.h>

#include "command/archiver.h"
#include "command/compiler.h"
#include "command/lowercase.h"
#include "command/option/clang.h"
#include "feature/cli.h"
#include "feature/handlers.h"
#include "feature/json.h"
#include "feature/path.h"
#include "session/handler.h"

namespace catter::feature {

namespace {

using kota::codec::dyn::Array;
using kota::codec::dyn::Object;

template <std::ranges::range Strings>
Array strings(Strings&& values) {
    Array array;
    for(const auto& value: values) {
        array.push_back(JSON(std::string_view(value)));
    }
    return array;
}

Object describe(const Target& target) {
    Object object;
    if(target.triple.has_value()) {
        object.insert("triple", *target.triple);
    }
    if(target.os.has_value()) {
        object.insert("os", name(*target.os));
    }
    if(target.object_format.has_value()) {
        object.insert("objectFormat", name(*target.object_format));
    }
    if(target.env.has_value()) {
        object.insert("env", name(*target.env));
    }
    object.insert("artifactModel", name(target.model));
    object.insert("from", name(target.source));
    return object;
}

/// How the command gives the path of a diagnostic: as a positional argument, as the value of an
/// option, or after `/link`, where the only paths a diagnostic is about are the outputs `/out`
/// names.
Object source_of(const Location& location, const CompilerAnalysis& analysis) {
    const auto& argument = analysis.arguments[location.argument];
    if(argument.id == option::clang::ID_INPUT) {
        return Object{
            {"kind", "argument"}
        };
    }
    if(argument.id != option::clang::ID__SLASH_link) {
        return Object{
            {"kind",        "option"         },
            {"option",      argument.spelling},
            {"optionIndex", argument.index   },
        };
    }
    const auto& values = argument.values;
    const auto value =
        *std::ranges::find_if(std::views::iota(0uz, values.size()), [&](std::size_t i) {
            return argument.value_index(i) == location.index;
        });
    const std::string_view token = values[value];
    const bool joined = lowercase(token).starts_with("/out:");
    return Object{
        {"kind", "remainder-option"},
        {"boundary", argument.spelling},
        {"boundaryIndex", argument.index},
        {"option", joined ? token.substr(0, 5) : std::string_view(values[value - 1])},
        {"optionIndex", argument.value_index(joined ? value : value - 1)},
    };
}

Object describe(const Diagnostic& diagnostic, const CompilerAnalysis& analysis) {
    Object object{
        {"code",    name(diagnostic.code)},
        {"message", diagnostic.message   },
    };
    if(diagnostic.path.has_value()) {
        object.insert("path", *diagnostic.path);
    }
    if(diagnostic.location.has_value()) {
        object.insert("index", diagnostic.location->index);
        object.insert("source", source_of(*diagnostic.location, analysis));
    }
    return object;
}

Object describe(std::string_view cwd,
                std::span<const std::string> argv,
                const CompilerAnalysis& analysis) {
    const auto path_of = [&](std::uint32_t read) -> std::string_view {
        return analysis.reads[read].path;
    };
    const auto is_source = [&](const Read& read) {
        return read.role == InputRole::Source;
    };

    Array edges;
    Array inferred;
    Array entries;
    for(const auto& write: analysis.writes) {
        edges.push_back(Object{
            {"output", write.path                                           },
            {"inputs", strings(write.reads | std::views::transform(path_of))},
        });
        if(write.origin != WriteOrigin::Explicit) {
            inferred.push_back(Object{
                {"path",   write.path                                           },
                {"reason",
                 write.origin == WriteOrigin::DefaultOutput ? "default-output"
                                                            : "assembly-listing"},
            });
        }
        if(write.path == "-") {
            continue;
        }
        for(auto read: write.reads) {
            if(is_source(analysis.reads[read])) {
                entries.push_back(Object{
                    {"directory", cwd},
                    {"file", analysis.reads[read].path},
                    {"arguments", strings(argv)},
                    {"output", absolute_path(cwd, write.path)},
                });
            }
        }
    }

    Object object{
        {"analyzer", "compiler"},
        {"compiler", std::format("builtin:{}", name(analysis.identity.compiler))},
        {"dialect", name(analysis.identity.dialect)},
        {"phase", name(analysis.phase)},
        {"artifact", name(analysis.artifact)},
        {"target", describe(analysis.target)},
        {"sources",
         strings(analysis.reads | std::views::filter(is_source) |
                 std::views::transform(&Read::path))},
        {"reads", strings(analysis.reads | std::views::transform(&Read::path))},
        {"writes", strings(analysis.writes | std::views::transform(&Write::path))},
        {"edges", std::move(edges)},
    };
    if(!inferred.empty()) {
        object.insert("inferred", std::move(inferred));
    }
    if(!analysis.diagnostics.empty()) {
        Array diagnostics;
        for(const auto& diagnostic: analysis.diagnostics) {
            diagnostics.push_back(describe(diagnostic, analysis));
        }
        object.insert("diagnostics", std::move(diagnostics));
    }
    object.insert("entries", std::move(entries));
    return object;
}

/// Each letter of the modifiers, a character of UTF-8 each.
Array letters(std::string_view modifiers) {
    Array array;
    for(std::size_t begin = 0; begin < modifiers.size();) {
        auto end = begin + 1;
        while(end < modifiers.size() &&
              (static_cast<unsigned char>(modifiers[end]) & 0xC0) == 0x80) {
            end += 1;
        }
        array.push_back(JSON(modifiers.substr(begin, end - begin)));
        begin = end;
    }
    return array;
}

Object describe(const ArchiverAnalysis& analysis) {
    Array edges;
    for(const auto& write: analysis.writes) {
        edges.push_back(Object{
            {"output", write                  },
            {"inputs", strings(analysis.reads)}
        });
    }
    return Object{
        {"analyzer",  "archiver"                 },
        {"operation", name(analysis.operation)   },
        {"modifiers", letters(analysis.modifiers)},
        {"thin",      analysis.thin              },
        {"archive",   analysis.archive           },
        {"members",   strings(analysis.members)  },
        {"reads",     strings(analysis.reads)    },
        {"writes",    strings(analysis.writes)   },
        {"edges",     std::move(edges)           },
    };
}

Object describe(const Error& error) {
    return Object{
        {"kind",    name(error.kind)},
        {"message", error.message   }
    };
}

/// What catter makes of the command `argv`, run in `cwd`: the analysis that recognizes it and
/// what it found, with the outputs it inferred, the notes on how, and the entries `cdb`
/// writes for it; or, when none does, why.
JSON describe(std::string_view cwd, std::span<const std::string> argv) {
    auto compiler = analyze_compiler(argv[0], argv);
    if(compiler.has_value()) {
        return describe(cwd, argv, *compiler);
    }
    auto archiver = analyze_archiver(argv[0], argv);
    if(archiver.has_value()) {
        return describe(*archiver);
    }
    return Object{
        {"analyzer", nullptr                                                      },
        {"errors",   Array{describe(compiler.error()), describe(archiver.error())}},
    };
}

struct Options {
    DECO_CFG_START(required = false)

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "inspect",
    .description = "Print how catter analyzes the command after --, without running it.",
    .examples = {{
        .command = "inspect -- clang++ -c src/main.cc -o main.o",
        .description = "The compiler, its mode, target, sources and outputs.",
    }},
};

/// Analyzes the build command, which it does not run, and prints what it makes of it.
class Inspect final : public Handler {
public:
    explicit Inspect(std::vector<std::string> args) : args(std::move(args)) {}

    kota::task<std::optional<Build>> on_start(Build build) override {
        if(!parse_options<Options>(help, args).has_value()) {
            co_return std::nullopt;
        }
        if(build.command.empty()) {
            throw cpptrace::runtime_error("inspect: give the command to analyze after --");
        }
        std::print("{}", to_json(describe(build.cwd, build.command)));
        co_return std::nullopt;
    }

    kota::task<Action> on_command(data::ipcid_t,
                                  std::optional<data::ipcid_t>,
                                  data::command) override {
        std::unreachable();
    }

    kota::task<> on_error(data::ipcid_t, std::optional<data::ipcid_t>, std::string) override {
        std::unreachable();
    }

    kota::task<> on_exit(data::ipcid_t, data::process_result) override {
        std::unreachable();
    }

    kota::task<> on_finish(data::process_result) override {
        std::unreachable();
    }

private:
    std::vector<std::string> args;
};

}  // namespace

std::unique_ptr<Handler> make_inspect(std::vector<std::string> args) {
    return std::make_unique<Inspect>(std::move(args));
}

}  // namespace catter::feature
