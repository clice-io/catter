#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "qjs.h"

namespace catter::js {

/**
 * Path-only ESM loader.
 *
 * Specifiers are resolved relative to the importing file. Explicit absolute paths are also
 * accepted. Extensions are never inferred and directory imports are rejected, matching Node's
 * ESM path rules. All loaded files are treated as ES modules by the caller.
 */
class EsmModuleLoader final : public qjs::Runtime::ModuleLoader {
public:
    explicit EsmModuleLoader(std::filesystem::path working_directory);

    std::string normalizer(const char* referrer_name, const char* module_name) override;
    qjs::Module loader(qjs::Context ctx, const char* module_name) override;

private:
    std::filesystem::path resolve_path(const char* referrer_name, const char* module_name) const;

    std::filesystem::path working_directory;
};
}  // namespace catter::js
