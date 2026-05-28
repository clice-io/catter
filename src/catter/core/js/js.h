#pragma once

#include <expected>
#include <filesystem>
#include <string_view>
#include <kota/async/runtime/task.h>

#include "async.h"
#include "qjs.h"
#include "capi/type.h"

namespace catter::js {

struct RuntimeConfig {
    std::filesystem::path pwd;
};

const RuntimeConfig& get_global_runtime_config();

class RuntimeScope {
public:
    RuntimeScope() = default;

    RuntimeScope(const RuntimeScope&) = delete;
    RuntimeScope& operator= (const RuntimeScope&) = delete;

    RuntimeScope(RuntimeScope&&) = delete;
    RuntimeScope& operator= (RuntimeScope&&) = delete;

    kota::task<> start(RuntimeConfig config);
    kota::task<> stop();

private:
    bool started = false;
};

kota::task<> run_script(std::string_view content, std::string_view filepath);

JsLoop& loop();

void set_on_start(qjs::Object cb);
void set_on_finish(qjs::Object cb);
void set_on_command(qjs::Object cb);
void set_on_execution(qjs::Object cb);

kota::task<CatterConfig> on_start(const CatterConfig& config);
kota::task<> on_finish(ProcessResult result);
kota::task<Action> on_command(uint32_t id, std::expected<CommandData, CatterErr> data);
kota::task<> on_execution(uint32_t id, ProcessResult result);

}  // namespace catter::js
