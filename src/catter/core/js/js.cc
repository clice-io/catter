#include "js.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <quickjs.h>
#include <cpptrace/exceptions.hpp>

#include "apitool.h"
#include "async.h"
#include "config/js-lib.h"

namespace catter::js {

using OnStart = qjs::Function<qjs::Promise(qjs::Object config)>;

using OnFinish = qjs::Function<qjs::Promise(qjs::Object event)>;

using OnCommand = qjs::Function<qjs::Promise(uint32_t id, qjs::Object data)>;

using OnExecution = qjs::Function<qjs::Promise(uint32_t id, qjs::Object data)>;

struct Self {
    RuntimeConfig global_config;
    qjs::Runtime rt;
    qjs::Object js_mod_obj;
    OnStart on_start;
    OnFinish on_finish;
    OnCommand on_command;
    OnExecution on_execution;
};

namespace {
Self self{};

template <typename T = void>
kota::task<T> wait_for_callback_promise(qjs::Promise promise) {
    auto result = co_await promise_to_task<T>(std::move(promise));
    if(!result) {
        throw std::move(result.error());
    }

    if constexpr(!std::is_void_v<T>) {
        co_return std::move(result).value();
    }
}
}  // namespace

namespace detail {

qjs::Runtime& runtime() {
    return self.rt;
}

void reset_runtime(const RuntimeConfig& config) {
    self.js_mod_obj = {};
    self.on_start = {};
    self.on_finish = {};
    self.on_command = {};
    self.on_execution = {};
    self.rt = qjs::Runtime::create();
    self.global_config = config;
}

void register_catter_module(const qjs::Context& ctx) {
    auto& mod = ctx.cmodule("catter-c");
    for(auto& reg: catter::apitool::api_registers()) {
        reg(mod, ctx);
    }
}

std::string_view js_lib_source() {
    auto last = config::data::js_lib.find_last_not_of('\0');
    if(last == std::string_view::npos) {
        return {};
    }
    return config::data::js_lib.substr(0, last + 1);
}

}  // namespace detail

const RuntimeConfig& get_global_runtime_config() {
    return self.global_config;
}

qjs::Object& js_mod_object() {
    return self.js_mod_obj;
}

bool drain_jobs_with_budget(qjs::Runtime& runtime, std::size_t max_jobs) {
    JSRuntime* js_rt = runtime.js_runtime();
    bool ran = false;
    JSContext* job_ctx = nullptr;

    for(std::size_t i = 0; i < max_jobs && JS_IsJobPending(js_rt); ++i) {
        int ret = JS_ExecutePendingJob(js_rt, &job_ctx);
        if(ret < 0) {
            if(job_ctx) {
                throw qjs::JSException::dump(job_ctx);
            }
            throw qjs::Exception("Error while executing pending JS job.");
        }
        if(ret == 0) {
            break;
        }
        ran = true;
    }

    return ran;
}

kota::task<CatterConfig> on_start(const CatterConfig& config) {
    if(!self.on_start) {
        throw cpptrace::runtime_error("service.onStart is not registered");
    }
    auto object = co_await wait_for_callback_promise<qjs::Object>(
        self.on_start(config.to_object(self.on_start.context())));
    co_return CatterConfig::make(std::move(object));
}

kota::task<> on_finish(ProcessResult result) {
    if(!self.on_finish) {
        throw cpptrace::runtime_error("service.onFinish is not registered");
    }
    co_await wait_for_callback_promise(self.on_finish(result.to_object(self.on_finish.context())));
}

kota::task<Action> on_command(uint32_t id, std::expected<CommandData, CatterErr> data) {
    if(!self.on_command) {
        throw cpptrace::runtime_error("service.onCommand is not registered");
    }
    auto command_result = qjs::Object::empty_one(self.on_command.context());
    if(data.has_value()) {
        command_result.set_property("success", true);
        command_result.set_property("data", data->to_object(self.on_command.context()));
    } else {
        command_result.set_property("success", false);
        command_result.set_property("error", data.error().to_object(self.on_command.context()));
    }
    auto object = co_await wait_for_callback_promise<qjs::Object>(
        self.on_command(id, std::move(command_result)));
    co_return Action::make(std::move(object));
}

kota::task<> on_execution(uint32_t id, ProcessResult result) {
    if(!self.on_execution) {
        throw cpptrace::runtime_error("service.onExecution is not registered");
    }
    co_await wait_for_callback_promise(
        self.on_execution(id, result.to_object(self.on_execution.context())));
}

void set_on_start(qjs::Object cb) {
    self.on_start = cb.as<OnStart>();
}

void set_on_finish(qjs::Object cb) {
    self.on_finish = cb.as<OnFinish>();
}

void set_on_command(qjs::Object cb) {
    self.on_command = cb.as<OnCommand>();
}

void set_on_execution(qjs::Object cb) {
    self.on_execution = cb.as<OnExecution>();
}

}  // namespace catter::js
