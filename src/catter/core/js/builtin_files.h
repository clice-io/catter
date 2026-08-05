#pragma once

#include <string_view>

namespace catter::js {

/**
 * Returns the source of a builtin module such as "catter" or "catter/cdb",
 * or an empty view when the module is unknown.
 */
std::string_view load_builtin_module(std::string_view module_name);

/**
 * Returns the source of a builtin script such as "script::cdb",
 * or an empty view when the script is unknown.
 */
std::string_view load_builtin_script(std::string_view script_name);

}  // namespace catter::js
