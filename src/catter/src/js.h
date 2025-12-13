#pragma once

#include "libqjs/qjs.h"
#include <filesystem>

namespace catter::core::js {

struct RuntimeConfig {
    std::filesystem::path pwd;
};

const RuntimeConfig& get_global_runtime_config();

/**
 * Initialize QuickJS runtime and context, register C++ APIs, and load JS libraries.
 * You can re-init it to reset the runtime and set new config, like pwd.
 * @throws std::runtime_error or std::exception if initialization fails.
 */
void init_qjs(const RuntimeConfig& config);

/**
 * Run a JavaScript file content in a new QuickJS runtime and context.
 *
 * @param content The JavaScript code to execute.
 * @param filename The name of the file (used for error reporting).
 * @throws qjs::Exception if there is an error during execution.
 */
void run_js_file(std::string_view content, const std::string filepath, bool check_error = true);

qjs::Object& js_mod_object();

/**
 * Get a property from the catter JS module as a specific type.
 *
 * @tparam T The expected type of the property (e.g., qjs::Function<void(std::string)>).
 * @param prop_name The name of the property to retrieve.
 * @return An optional containing the property if it exists(no exception happens, and isnot
 * undefined) and can be converted to type T; std::nullopt otherwise.It maybe js `null` even return
 * a T.
 */
template <typename T>
std::optional<T> prop_of_js_mod(const std::string& prop_name) {
    auto val = js_mod_object().get_optional_property(prop_name);
    if(!val.has_value()) {
        return std::nullopt;
    }
    if constexpr(std::is_same_v<T, qjs::Value>) {
        return val;
    } else if constexpr(catter::meta::is_specialization_of_v<T, qjs::Function>) {
        if(!val->is_function()) {
            return std::nullopt;
        }
        auto to_obj_res = val->to<qjs::Object>();
        if(!to_obj_res.has_value()) {
            return std::nullopt;
        }
        auto to_func_res = to_obj_res->to<T>();
        return to_func_res;
    } else {
        return val->to<T>();
    }
}

};  // namespace catter::core::js
