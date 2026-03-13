#pragma once
#include <filesystem>
#include <variant>

#include "qjs.h"

#include "capi/type.h"

namespace catter::js {

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
void run_js_file(std::string_view content, const std::string filepath);

qjs::Object& js_mod_object();

void set_on_start(qjs::Object cb);
void set_on_finish(qjs::Object cb);
void set_on_command(qjs::Object cb);
void set_on_execution(qjs::Object cb);

CatterConfig on_start(CatterConfig config);
void on_finish();
Action on_command(uint32_t id, std::variant<CommandData, CatterErr> data);
void on_execution(uint32_t id, ExecutionEvent event);

};  // namespace catter::js
