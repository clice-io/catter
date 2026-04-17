#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <quickjs.h>
#include <string>
#include <string_view>
#include <vector>

#include "qjs.h"
#include "../apitool.h"
#include "opt/external/clang.h"
#include "opt/external/lld_coff.h"
#include "opt/external/lld_elf.h"
#include "opt/external/lld_macho.h"
#include "opt/external/lld_mingw.h"
#include "opt/external/lld_wasm.h"
#include "opt/external/llvm_dlltool.h"
#include "opt/external/llvm_lib.h"
#include "opt/external/nvcc.h"

#include <kota/option/option.h>

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

catter::qjs::Object make_option_item(JSContext* ctx,
                                     [[maybe_unused]] const eo::OptTable& table,
                                     const eo::ParsedArgument& arg) {
    auto item = catter::qjs::Object::empty_one(ctx);

    item.set_property("values",
                      catter::qjs::Array<std::string>::from(ctx, copy_values(arg.values)));
    item.set_property("key", std::string(arg.get_spelling_view()));
    item.set_property("id", static_cast<uint32_t>(arg.option_id.id()));
    item.set_property("index", static_cast<uint32_t>(arg.index));
    if(arg.unaliased_option_id.has_value()) {
        item.set_property("unalias", static_cast<uint32_t>(arg.unaliased_option_id->id()));
    }
    return item;
}

bool emit_callback_value(OptionParseCallback& callback, catter::qjs::Value value) {
    catter::qjs::Parameters args;
    args.emplace_back(std::move(value));
    return callback(std::move(args));
}

bool matches_visibility(const eo::OptTable& table,
                        const eo::ParsedArgument& arg,
                        uint32_t visibility) {
    if(visibility == kAllOptionVisibility) {
        return true;
    }

    auto option = table.option(arg.option_id);
    if(!option.valid()) {
        return true;
    }

    const auto& info = table.options()[option.id() - 1];
    return (static_cast<uint32_t>(info.visibility) & visibility) != 0;
}

CTX_CAPI(option_get_info,
         (JSContext * ctx, std::string table_name, unsigned int id)->catter::qjs::Object) {
    using namespace catter;
    auto& table = resolve_table(table_name);
    const auto& option = table.option(id);
    if(!option.valid()) {
        throw qjs::Exception(std::format("Invalid option id {} for table {}", id, table_name));
    }

    const auto& info = table.options()[option.id() - 1];
    auto res = qjs::Object::empty_one(ctx);
    res.set_property("id", static_cast<uint32_t>(info.id));
    res.set_property("prefixedKey", std::string(option.prefixed_name()));
    res.set_property("kind", static_cast<uint32_t>(info.kind));
    res.set_property("group", static_cast<uint32_t>(info.group_id));
    res.set_property("alias", static_cast<uint32_t>(info.alias_id));
    res.set_property("aliasArgs",
                     qjs::Array<std::string>::from(ctx, split_alias_args(info.alias_args)));
    res.set_property("flags", static_cast<uint32_t>(info.flags));
    res.set_property("visibility", static_cast<uint32_t>(info.visibility));
    res.set_property("param", static_cast<uint32_t>(info.param));
    res.set_property("help", std::string(info.help_text != nullptr ? info.help_text : ""));
    res.set_property("meta_var", std::string(info.meta_var != nullptr ? info.meta_var : ""));
    return res;
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

    table.parse_args(args, [&](std::expected<eo::ParsedArgument, std::string> parsed) -> bool {
        if(parsed.has_value()) {
            if(!matches_visibility(table, *parsed, visibility)) {
                return true;
            }
            return emit_callback_value(
                callback,
                catter::qjs::Value::from(make_option_item(ctx, table, *parsed)));
        }
        return emit_callback_value(callback, catter::qjs::Value::from(ctx, parsed.error()));
    });
}

}  // namespace
