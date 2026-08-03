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
#include "option/clang.h"
#include "option/lld_coff.h"
#include "option/lld_elf.h"
#include "option/lld_macho.h"
#include "option/lld_mingw.h"
#include "option/lld_wasm.h"
#include "option/llvm_dlltool.h"
#include "option/llvm_lib.h"
#include "option/nvcc.h"

namespace {

using namespace catter;
using OptionParseCallback = qjs::Function<bool(qjs::Parameters)>;

namespace kota_opt = kota::option;

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

const kota_opt::OptTable& resolve_table(std::string_view table_name) {
#define RESOLVE_OPTION_TABLE(NAME, NS)                                                             \
    if(table_name == NAME) {                                                                       \
        return opt::NS::table();                                                                   \
    }
    CAPI_OPTION_TABLES(RESOLVE_OPTION_TABLE)
#undef RESOLVE_OPTION_TABLE

    throw qjs::Exception(std::format("Unknown option table: {}", table_name));
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

js::OptionItem make_option_item([[maybe_unused]] const kota_opt::OptTable& table,
                                const kota_opt::ParsedArg& arg) {
    js::OptionItem item{
        .values = copy_values(arg.values),
        .key = std::string(arg.spelling),
        .id = arg.id,
        .index = static_cast<uint32_t>(arg.index),
    };
    return item;
}

CTX_CAPI(option_get_info, (JSContext * ctx, std::string table_name, unsigned int id)->qjs::Object) {
    using namespace catter;
    auto& table = resolve_table(table_name);

    if(id == 0 || id > table.option_infos.size()) {
        throw qjs::Exception(std::format("Invalid option id {} for table {}", id, table_name));
    }

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

CTX_CAPI(option_parse, (JSContext * ctx, qjs::Parameters params)->void) {
    if(params.size() != 3 && params.size() != 4) {
        throw qjs::Exception(
            std::format("option_parse expects 3 or 4 arguments, got {}", params.size()));
    }

    auto table_name = params[0].as<std::string>();
    auto args_object = params[1].as<qjs::Object>();
    auto callback_object = params[2].as<qjs::Object>();
    uint32_t visibility = kAllOptionVisibility;
    if(params.size() == 4 && !params[3].is_nothing()) {
        visibility = params[3].as<uint32_t>();
    }

    auto args = args_object.as<qjs::Array<std::string>>().as<std::vector<std::string>>();
    auto callback = callback_object.as<OptionParseCallback>();
    const auto& table = resolve_table(table_name);

    kota_opt::ParseOptions options;
    options.dash_dash_parsing = true;
    options.visibility = visibility;

    for(auto result: table.parse(args, options)) {
        if(!result.has_value()) {
            const auto& error = result.error();
            const auto failing_arg = error.index < args.size() ? std::string_view(args[error.index])
                                                               : std::string_view("<end-of-argv>");
            const auto reason = error.message != nullptr ? error.message : "missing argument";

            callback({qjs::Value::from(ctx,
                                       std::format("failed to parse '{}' (arg #{}) : {}",
                                                   failing_arg,
                                                   error.index,
                                                   reason))});
            return;
        }

        const auto& parsed = *result;
        if(!callback({qjs::Value::from(make_option_item(table, parsed).to_object(ctx))})) {
            return;
        }
    }
}

}  // namespace
