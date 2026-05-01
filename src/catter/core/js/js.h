#pragma once
#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "qjs.h"
#include "capi/type.h"

namespace catter::js {

struct RuntimeConfig {
    std::filesystem::path pwd;
};

const RuntimeConfig& get_global_runtime_config();

qjs::Object& js_mod_object();

bool drain_jobs_with_budget(qjs::Runtime& runtime, std::size_t max_jobs = 64);

void set_on_start(qjs::Object cb);
void set_on_finish(qjs::Object cb);
void set_on_command(qjs::Object cb);
void set_on_execution(qjs::Object cb);

CatterConfig on_start(CatterConfig config);
void on_finish(ProcessResult result);
Action on_command(uint32_t id, std::expected<CommandData, CatterErr> data);
void on_execution(uint32_t id, ProcessResult result);

namespace detail {

qjs::Runtime& runtime();
void reset_runtime(const RuntimeConfig& config);
void register_catter_module(const qjs::Context& ctx);
std::string_view js_lib_source();
std::string format_rejection(qjs::Parameters& args);
qjs::Promise promise_from_eval_result(qjs::Value&& eval_result);

}  // namespace detail

};  // namespace catter::js
