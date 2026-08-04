#include "esm_loader.h"

#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <string_view>
#include <unordered_map>

namespace catter::js {

namespace {

constexpr std::string_view kJsLibPrefix = "catter";

bool is_path_specifier(std::string_view specifier) {
    return specifier.starts_with("./") || specifier.starts_with("../") || specifier == "." ||
           specifier == ".." || std::filesystem::path(specifier).is_absolute();
}

extern "C" {
    extern const char _binary_jslib_bin_start[];
    extern const char _binary_jslib_bin_end[];
}

uint32_t read_u32(const char*& cursor, const char* end) {
    if(cursor + sizeof(uint32_t) > end) {
        return 0;
    }
    uint32_t value;
    std::memcpy(&value, cursor, sizeof(value));
    cursor += sizeof(value);
    return value;
}

struct BuiltinTable {
    std::unordered_map<std::string, std::string_view> modules;
};

/**
 * Parses the embedded jslib.bin blob once.
 *
 * Layout (little-endian): u32 count, then per module:
 *   u32 name_len, name bytes, u32 content_len, content bytes.
 * All views point into the blob, which lives for the process lifetime.
 */
const BuiltinTable& builtin_table() {
    const static BuiltinTable table = [] {
        BuiltinTable table;
        const char* cursor = _binary_jslib_bin_start;
        const char* end = _binary_jslib_bin_end;
        const uint32_t count = read_u32(cursor, end);
        for(uint32_t i = 0; i < count; ++i) {
            const uint32_t name_len = read_u32(cursor, end);
            if(name_len == 0 || cursor + name_len > end) {
                break;
            }
            const std::string_view name{cursor, name_len};
            cursor += name_len;

            const uint32_t content_len = read_u32(cursor, end);
            if(cursor + content_len > end) {
                break;
            }
            const std::string_view content{cursor, content_len};
            cursor += content_len;

            table.modules.emplace(name, content);

            // Each content view is followed by a NUL separator in the blob so
            // the view is NUL-terminated in memory (QuickJS's tokenizer relies
            // on NUL to detect end of input).
            if(cursor < end) {
                ++cursor;
            }
        }
        return table;
    }();
    return table;
}

}  // namespace

std::string_view load_builtin_module(std::string_view module_name) {
    const auto& table = builtin_table();
    if(auto it = table.modules.find(std::string(module_name)); it != table.modules.end()) {
        return it->second;
    }
    return {};
}

std::filesystem::path EsmModuleLoader::resolve_path(std::string_view referrer_name,
                                                    std::string_view module_name) const {

    if(!is_path_specifier(module_name)) {
        throw qjs::Exception("Unsupported ESM module specifier '{}'; only file paths are supported",
                             module_name);
    }

    auto referrer = std::filesystem::path(referrer_name);

    if(referrer.is_relative()) {
        throw qjs::Exception(
            "Referrer path '{}' is not absolute; only absolute paths are supported",
            referrer_name);
    }

    std::filesystem::path path = referrer.parent_path() / std::filesystem::path(module_name);

    return path.lexically_normal().string();
}

std::string EsmModuleLoader::normalizer(std::string_view referrer_name,
                                        std::string_view module_name) {
    if(module_name.starts_with(kJsLibPrefix)) {
        // Builtin specifiers (and the native "catter-c" module) keep their name as the canonical
        // module name.
        return std::string(module_name);
    }

    auto resolved = resolve_path(referrer_name, module_name);

    std::error_code ec;
    const bool exists = std::filesystem::exists(resolved, ec);
    if(ec || !exists) {
        throw qjs::Exception("Cannot find module '{}' imported from '{}'",
                             module_name,
                             referrer_name);
    }
    if(std::filesystem::is_directory(resolved, ec)) {
        throw qjs::Exception("Directory import '{}' is not supported", resolved.string());
    }
    if(ec || !std::filesystem::is_regular_file(resolved, ec) || ec) {
        throw qjs::Exception("Cannot load module '{}'", resolved.string());
    }
    return resolved.string();
}

qjs::Module EsmModuleLoader::loader(qjs::Context ctx, std::string_view module_name) {
    if(module_name.starts_with(kJsLibPrefix)) {
        const auto source = load_builtin_module(module_name);
        if(source.empty()) {
            throw qjs::Exception("Unknown builtin module '{}'", module_name);
        }
        return ctx.load_module(source, module_name.data());
    }

    std::filesystem::path path = module_name;
    std::ifstream input(path, std::ios::binary);
    if(!input) {
        throw qjs::Exception("Failed to read module '{}'", path.string());
    }
    std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    return ctx.load_module(source, module_name.data());
}
}  // namespace catter::js
