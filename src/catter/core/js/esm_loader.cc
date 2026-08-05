#include "esm_loader.h"

#include <format>
#include <fstream>
#include <iterator>
#include <string_view>

#include "builtin_files.h"

namespace catter::js {

namespace {

constexpr std::string_view kJsLibPrefix = "catter";

bool is_path_specifier(std::string_view specifier) {
    return specifier.starts_with("./") || specifier.starts_with("../") || specifier == "." ||
           specifier == ".." || std::filesystem::path(specifier).is_absolute();
}

}  // namespace

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
        const auto source = catter::js::load_builtin_module(module_name);
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
