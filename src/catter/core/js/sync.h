#pragma once

#include <string>
#include <string_view>

#include "js.h"

namespace catter::js {

/**
 * Initialize QuickJS runtime and context, register C++ APIs, and load JS libraries.
 * You can re-init it to reset the runtime and set new config, like pwd.
 */
void init_qjs(const RuntimeConfig& config);

void sync_eval(std::string_view input, const char* filename, int eval_flags);

/**
 * Run JavaScript module content in the current QuickJS runtime.
 *
 * @param content The JavaScript code to execute.
 * @param filepath The file path used for error reporting.
 * @throws qjs::Exception if there is an error during execution.
 */
void run_js_file(std::string_view content, const std::string filepath);

}  // namespace catter::js
