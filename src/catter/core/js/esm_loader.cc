#include "esm_loader.h"

#include <format>
#include <fstream>
#include <iterator>
#include <string_view>

namespace catter::js {

namespace {

bool is_path_specifier(std::string_view specifier) {
    return specifier.starts_with("./") || specifier.starts_with("../") || specifier == "." ||
           specifier == ".." || std::filesystem::path(specifier).is_absolute();
}

}  // namespace

extern "C" {
    extern const char _binary_lib_js_start[];
    extern const char _binary_lib_js_end[];
}

std::string_view js_lib_source() {
    const std::string_view js_lib{_binary_lib_js_start, _binary_lib_js_end};
    auto last = js_lib.find_last_not_of('\0');
    if(last == std::string_view::npos) {
        return {};
    }
    return js_lib.substr(0, last + 1);
}

std::string_view load_builtin_module(std::string_view module_name) {
    if(module_name == "catter") {
        return js_lib_source();
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
    if(module_name.starts_with("catter")) {
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
    if(module_name.starts_with("catter")) {
        return ctx.load_module(load_builtin_module(module_name), module_name.data());
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
