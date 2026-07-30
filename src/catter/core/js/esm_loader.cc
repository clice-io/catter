#include "esm_loader.h"

#include <format>
#include <fstream>
#include <iterator>

namespace catter::js {

namespace {

bool is_path_specifier(std::string_view specifier) {
    return specifier.starts_with("./") || specifier.starts_with("../") || specifier == "." ||
           specifier == ".." || std::filesystem::path(specifier).is_absolute();
}

std::filesystem::path absolute_normalized(std::filesystem::path path,
                                          const std::filesystem::path& working_directory) {
    if(path.is_relative()) {
        path = working_directory / std::move(path);
    }
    return std::filesystem::absolute(path).lexically_normal();
}

}  // namespace

EsmModuleLoader::EsmModuleLoader(std::filesystem::path working_directory) :
    working_directory(std::filesystem::absolute(std::move(working_directory)).lexically_normal()) {}

std::filesystem::path EsmModuleLoader::resolve_path(const char* referrer_name,
                                                    const char* module_name) const {
    const std::string_view specifier{module_name ? module_name : ""};

    if(specifier.starts_with("catter")) {
        return specifier;
    }

    if(!is_path_specifier(specifier)) {
        throw qjs::Exception("Unsupported ESM module specifier '{}'; only file paths are supported",
                             specifier);
    }

    std::filesystem::path base = this->working_directory;
    if(referrer_name && *referrer_name) {
        auto referrer = std::filesystem::path(referrer_name);
        if(referrer.is_relative()) {
            referrer = this->working_directory / std::move(referrer);
        }
        base = referrer.parent_path();
    }

    auto resolved = absolute_normalized(std::filesystem::path(specifier), base);
    std::error_code ec;
    const bool exists = std::filesystem::exists(resolved, ec);
    if(ec || !exists) {
        throw qjs::Exception("Cannot find module '{}' imported from '{}'",
                             specifier,
                             referrer_name ? referrer_name : "<entry>");
    }
    if(std::filesystem::is_directory(resolved, ec)) {
        throw qjs::Exception("Directory import '{}' is not supported", resolved.string());
    }
    if(ec || !std::filesystem::is_regular_file(resolved, ec) || ec) {
        throw qjs::Exception("Cannot load module '{}'", resolved.string());
    }
    return resolved;
}

std::string EsmModuleLoader::normalizer(const char* referrer_name, const char* module_name) {
    return resolve_path(referrer_name, module_name).string();
}

qjs::Module EsmModuleLoader::loader(qjs::Context ctx, const char* module_name) {
    const auto path = resolve_path(nullptr, module_name);
    std::ifstream input(path, std::ios::binary);
    if(!input) {
        throw qjs::Exception("Failed to read module '{}'", path.string());
    }
    std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    return ctx.load_module(source, module_name);
}
}  // namespace catter::js
