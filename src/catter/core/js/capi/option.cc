#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <quickjs.h>
#include <kota/deco/option.h>

#include "type.h"
#include "../apitool.h"
#include "../qjs.h"
#include "opt/external/clang.h"
#include "opt/external/lld_coff.h"
#include "opt/external/lld_elf.h"
#include "opt/external/lld_macho.h"
#include "opt/external/lld_mingw.h"
#include "opt/external/lld_wasm.h"
#include "opt/external/llvm_dlltool.h"
#include "opt/external/llvm_lib.h"
#include "opt/external/nvcc.h"

namespace {

namespace eo = kota::option;

using OptionParseCallback = catter::qjs::Function<bool(catter::qjs::Parameters)>;
constexpr uint32_t kAllOptionVisibility = std::numeric_limits<uint32_t>::max();

#define CAPI_OPTION_TABLES(X)                                                                      \
    X("clang", clang)                                                                              \
    X("lld-coff", lld_coff)                                                                        \
    X("lld-elf", lld_elf)                                                                          \
    X("lld-macho", lld_macho)                                                                      \
    X("lld-mingw", lld_mingw)                                                                      \
    X("lld-wasm", lld_wasm)                                                                        \
    X("nvcc", nvcc)                                                                                \
    X("llvm-dlltool", llvm_dlltool)                                                                \
    X("llvm-lib", llvm_lib)

const eo::OptTable& resolve_table(std::string_view table_name) {
#define RESOLVE_OPTION_TABLE(NAME, NS)                                                             \
    if(table_name == NAME) {                                                                       \
        return catter::opt::NS::table();                                                           \
    }
    CAPI_OPTION_TABLES(RESOLVE_OPTION_TABLE)
#undef RESOLVE_OPTION_TABLE

    throw catter::qjs::Exception(std::format("Unknown option table: {}", table_name));
}

std::vector<std::string> copy_values(std::span<const std::string_view> values) {
    std::vector<std::string> copied;
    copied.reserve(values.size());
    for(auto value: values) {
        copied.emplace_back(value);
    }
    return copied;
}

std::vector<std::string> split_alias_args(const char* alias_args) {
    std::vector<std::string> result;
    if(alias_args == nullptr) {
        return result;
    }
    while(*alias_args != '\0') {
        result.emplace_back(alias_args);
        alias_args += std::strlen(alias_args) + 1;
    }
    return result;
}

catter::js::OptionItem make_option_item([[maybe_unused]] const eo::OptTable& table,
                                        const eo::ParsedArg& arg) {
    catter::js::OptionItem item{
        .values = copy_values(arg.values),
        .key = std::string(arg.spelling),
        .id = arg.id,
        .index = static_cast<uint32_t>(arg.index),
    };
    return item;
}

std::uint32_t match_option_length(const eo::Option& option,
                                  std::string_view argument,
                                  bool ignore_case) {
    const auto name = option.name();
    for(auto prefix: option.prefixes) {
        if(!argument.starts_with(prefix)) {
            continue;
        }
        auto rest = argument.substr(prefix.size());
        bool matched =
            ignore_case ? rest.size() >= name.size() &&
                              std::equal(name.begin(),
                                         name.end(),
                                         rest.begin(),
                                         [](char a, char b) {
                                             return std::tolower(static_cast<unsigned char>(a)) ==
                                                    std::tolower(static_cast<unsigned char>(b));
                                         })
                        : rest.starts_with(name);
        if(matched) {
            return static_cast<std::uint32_t>(prefix.size() + name.size());
        }
    }
    return 0;
}

bool is_hidden_argument(const eo::OptTable& table, std::string_view spelling, uint32_t visibility) {
    if(visibility == kAllOptionVisibility) {
        return false;
    }

    // The parsed argument's id is already the canonical (unaliased) option, so
    // find the original option entry that matched `spelling` and check its own
    // visibility. Options whose visibility does not intersect the requested
    // mask are consumed by the parser but hidden from the result.
    const auto canonical = table.find_option(spelling, kAllOptionVisibility);
    if(!canonical.has_value()) {
        return false;
    }
    bool any_visible = false;
    for(const auto& option: table.option_infos) {
        auto matched_length = match_option_length(option, spelling, table.ignore_case);
        if(matched_length == 0) {
            continue;
        }
        if(matched_length < spelling.size()) {
            // Partial prefix matches only count for joined-style options.
            switch(option.kind) {
                case eo::Kind::Joined:
                case eo::Kind::CommaJoined:
                case eo::Kind::JoinedOrSeparate:
                case eo::Kind::JoinedAndSeparate:
                case eo::Kind::RemainingArgsJoined: break;
                default: continue;
            }
        }
        if(option.alias_id != 0 && option.alias_id != canonical->id()) {
            continue;
        }
        if(option.visibility & visibility) {
            any_visible = true;
        }
    }
    return !any_visible;
}

bool emit_callback_value(OptionParseCallback& callback, catter::qjs::Value value) {
    catter::qjs::Parameters args;
    args.emplace_back(std::move(value));
    return callback(std::move(args));
}

CTX_CAPI(option_get_info,
         (JSContext * ctx, std::string table_name, unsigned int id)->catter::qjs::Object) {
    using namespace catter;
    auto& table = resolve_table(table_name);
    const auto& option = table.option(id);
    if(!option.has_value()) {
        throw qjs::Exception(std::format("Invalid option id {} for table {}", id, table_name));
    }

    const auto& info = *option;
    return js::OptionInfo{
        .id = info.id(),
        .prefixedKey = std::string(info.prefixed_name()),
        .kind = static_cast<uint32_t>(info.kind()),
        .group = static_cast<uint32_t>(info.group().has_value() ? info.group()->id() : 0),
        .alias = static_cast<uint32_t>(info.alias().has_value() ? info.alias()->id() : 0),
        .aliasArgs = split_alias_args(info.alias_args()),
        .flags = table.option_infos[info.id() - 1].flags,
        .visibility = table.option_infos[info.id() - 1].visibility,
        .param = info.num_args(),
        .help = std::string(table.option_infos[info.id() - 1].help_text != nullptr
                                ? table.option_infos[info.id() - 1].help_text
                                : ""),
        .meta_var = std::string(table.option_infos[info.id() - 1].meta_var != nullptr
                                    ? table.option_infos[info.id() - 1].meta_var
                                    : ""),
    }
        .to_object(ctx);
};

CTX_CAPI(option_parse, (JSContext * ctx, catter::qjs::Parameters params)->void) {
    if(params.size() != 3 && params.size() != 4) {
        throw catter::qjs::Exception(
            std::format("option_parse expects 3 or 4 arguments, got {}", params.size()));
    }

    auto table_name = params[0].as<std::string>();
    auto args_object = params[1].as<catter::qjs::Object>();
    auto callback_object = params[2].as<catter::qjs::Object>();
    uint32_t visibility = kAllOptionVisibility;
    if(params.size() == 4 && !params[3].is_nothing()) {
        visibility = params[3].as<uint32_t>();
    }

    auto args = args_object.as<catter::qjs::Array<std::string>>().as<std::vector<std::string>>();
    auto callback = callback_object.as<OptionParseCallback>();
    const auto& table = resolve_table(table_name);

    eo::ParseOptions options;
    options.dash_dash_parsing = true;
    options.visibility = visibility;

    for(auto result: table.parse(args, options)) {
        if(!result.has_value()) {
            const auto& error = result.error();
            const auto failing_arg = error.index < args.size() ? std::string_view(args[error.index])
                                                               : std::string_view("<end-of-argv>");
            const auto reason = error.message != nullptr ? error.message : "missing argument";
            emit_callback_value(
                callback,
                catter::qjs::Value::from(ctx,
                                         std::format("failed to parse '{}' (arg #{}) : {}",
                                                     failing_arg,
                                                     error.index,
                                                     reason)));
            return;
        }

        const auto& parsed = *result;
        if(is_hidden_argument(table, parsed.spelling, visibility)) {
            continue;
        }

        if(!emit_callback_value(
               callback,
               catter::qjs::Value::from(make_option_item(table, parsed).to_object(ctx)))) {
            return;
        }
    }
}

}  // namespace
