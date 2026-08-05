#include "builtin_files.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>

namespace catter::js {

namespace {

extern "C" {
    extern const char _binary_jslib_bin_start[];
    extern const char _binary_jslib_bin_end[];
    extern const char _binary_scripts_bin_start[];
    extern const char _binary_scripts_bin_end[];
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
    std::unordered_map<std::string, std::string_view> entries;
};

/**
 * Parses an embedded blob once.
 *
 * Layout (little-endian): u32 count, then per entry:
 *   u32 name_len, name bytes, u32 content_len, content bytes.
 * All views point into the blob, which lives for the process lifetime.
 */
BuiltinTable parse_blob(const char* start, const char* end) {
    BuiltinTable table;
    const char* cursor = start;
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

        table.entries.emplace(name, content);

        // Each content view is followed by a NUL separator in the blob so
        // the view is NUL-terminated in memory (QuickJS's tokenizer relies
        // on NUL to detect end of input).
        if(cursor < end) {
            ++cursor;
        }
    }
    return table;
}

struct BuiltinFiles {
    BuiltinTable modules;
    BuiltinTable scripts;
};

const BuiltinFiles& builtin_files() {
    const static BuiltinFiles files = [] {
        BuiltinFiles files;
        files.modules = parse_blob(_binary_jslib_bin_start, _binary_jslib_bin_end);
        files.scripts = parse_blob(_binary_scripts_bin_start, _binary_scripts_bin_end);
        return files;
    }();
    return files;
}

}  // namespace

std::string_view load_builtin_module(std::string_view module_name) {
    const auto& table = builtin_files().modules;
    if(auto it = table.entries.find(std::string(module_name)); it != table.entries.end()) {
        return it->second;
    }
    return {};
}

std::string_view load_builtin_script(std::string_view script_name) {
    const auto& table = builtin_files().scripts;
    if(auto it = table.entries.find(std::string(script_name)); it != table.entries.end()) {
        return it->second;
    }
    return {};
}

}  // namespace catter::js
