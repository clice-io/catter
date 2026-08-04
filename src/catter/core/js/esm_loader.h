#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "qjs.h"

namespace catter::js {

/**
 * Returns the source of a builtin module such as "catter" or "catter/cdb",
 * or an empty view when the module is unknown.
 */
std::string_view load_builtin_module(std::string_view module_name);

/**
 * Path-only ESM loader.
 *
 * Specifiers are resolved relative to the importing file. Explicit absolute paths are also
 * accepted. Extensions are never inferred and directory imports are rejected, matching Node's
 * ESM path rules. All loaded files are treated as ES modules by the caller.
 */
class EsmModuleLoader final : public qjs::Runtime::ModuleLoader {
public:
    std::string normalizer(std::string_view referrer_name, std::string_view module_name) override;
    qjs::Module loader(qjs::Context ctx, std::string_view module_name) override;

private:
    std::filesystem::path resolve_path(std::string_view referrer_name,
                                       std::string_view module_name) const;
};
}  // namespace catter::js
