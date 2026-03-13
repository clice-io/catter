#pragma once
#include <filesystem>

#include <eventide/common/meta.h>

#include "qjs.h"

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

};  // namespace catter::core::js
