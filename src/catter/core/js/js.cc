#include "js.h"

#include <cassert>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <quickjs.h>
#include <cpptrace/exceptions.hpp>

#include "apitool.h"
#include "async.h"
#include "esm_loader.h"

extern "C" {
    extern const char _binary_lib_js_start[];
    extern const char _binary_lib_js_end[];
}

namespace catter::js {

namespace {

using OnStart = qjs::Function<qjs::Promise(qjs::Object config)>;

using OnFinish = qjs::Function<qjs::Promise(qjs::Object event)>;

using OnCommand = qjs::Function<qjs::Promise(uint32_t id, qjs::Object data)>;

using OnExecution = qjs::Function<qjs::Promise(uint32_t id, qjs::Object data)>;

struct RuntimeState {
    RuntimeConfig config;
    qjs::Runtime runtime;
    JsLoop js_loop{64};
    OnStart on_start;
    OnFinish on_finish;
    OnCommand on_command;
    OnExecution on_execution;

    void reset(RuntimeConfig next_config) {
        on_start = {};
        on_finish = {};
        on_command = {};
        on_execution = {};
        runtime = qjs::Runtime::create();
        runtime.set_module_loader(std::make_unique<EsmModuleLoader>(next_config.pwd));
        config = std::move(next_config);
    }
};

RuntimeState state{};

std::string_view js_lib_source() {
    const std::string_view js_lib{_binary_lib_js_start, _binary_lib_js_end};
    auto last = js_lib.find_last_not_of('\0');
    if(last == std::string_view::npos) {
        return {};
    }
    return js_lib.substr(0, last + 1);
}

kota::task<> eval_module(std::string_view input, const char* filename) {
    auto ctx = state.runtime.context();
    auto result = co_await state.js_loop.promise_to_task<void>(ctx.eval_module(input, filename));
    if(!result) {
        throw result.error().to_exception();
    }
    co_return;
}

template <typename T = void>
kota::task<T> wait_for_callback_promise(qjs::Promise promise) {
    auto result = co_await state.js_loop.promise_to_task<T>(std::move(promise));
    if(!result) {
        throw result.error().to_exception();
    }

    if constexpr(!std::is_void_v<T>) {
        co_return std::move(result).value();
    }
}

}  // namespace

const RuntimeConfig& get_global_runtime_config() {
    return state.config;
}

kota::task<> RuntimeScope::start(RuntimeConfig config) {
    if(started) {
        throw qjs::Exception("QuickJS runtime scope is already started.");
    }
    if(!state.js_loop.is_stopped()) {
        throw qjs::Exception("QuickJS async loop is already running.");
    }

    state.reset(std::move(config));

    auto& loop = kota::event_loop::current();
    auto loop_task = state.js_loop.run(state.runtime, loop);
    loop.schedule(std::move(loop_task));

    std::exception_ptr error;
    try {
        const auto& ctx = state.runtime.context();
        auto& mod = ctx.cmodule("catter-c");
        for(auto& reg: catter::apitool::api_registers()) {
            reg(mod, ctx);
        }
        co_await eval_module(js_lib_source(), "catter");
    } catch(...) {
        error = std::current_exception();
    }

    if(error) {
        co_await state.js_loop.stop();
        std::rethrow_exception(error);
    }

    started = true;
    co_return;
}

kota::task<> RuntimeScope::stop() {
    if(!started) {
        co_return;
    }

    co_await state.js_loop.stop();
    started = false;
    co_return;
}

kota::task<> run_script(std::string_view content, std::string_view filepath) {
    auto filename = std::string(filepath);
    co_await eval_module(content, filename.c_str());
    co_return;
}

JsLoop& loop() {
    return state.js_loop;
}

kota::task<CatterConfig> on_start(const CatterConfig& config) {
    if(!state.on_start) {
        throw cpptrace::runtime_error("service.onStart is not registered");
    }
    auto object = co_await wait_for_callback_promise<qjs::Object>(
        state.on_start(config.to_object(state.on_start.context())));
    co_return CatterConfig::make(std::move(object));
}

kota::task<> on_finish(ProcessResult result) {
    if(!state.on_finish) {
        throw cpptrace::runtime_error("service.onFinish is not registered");
    }
    co_await wait_for_callback_promise(
        state.on_finish(result.to_object(state.on_finish.context())));
    co_return;
}

kota::task<Action> on_command(uint32_t id, std::expected<CommandData, CatterErr> data) {
    if(!state.on_command) {
        throw cpptrace::runtime_error("service.onCommand is not registered");
    }

    // TODO
    // Add a helper function to convert std::expected to a JS object in a more generic way, and
    // reuse it in other places.

    auto command_result = qjs::Object::empty_one(state.on_command.context());
    if(data.has_value()) {
        command_result.set_property("success", true);
        command_result.set_property("data", data->to_object(state.on_command.context()));
    } else {
        command_result.set_property("success", false);
        command_result.set_property("error", data.error().to_object(state.on_command.context()));
    }
    auto object = co_await wait_for_callback_promise<qjs::Object>(
        state.on_command(id, std::move(command_result)));
    co_return Action::make(std::move(object));
}

kota::task<> on_execution(uint32_t id, ProcessResult result) {
    if(!state.on_execution) {
        throw cpptrace::runtime_error("service.onExecution is not registered");
    }
    co_await wait_for_callback_promise(
        state.on_execution(id, result.to_object(state.on_execution.context())));
    co_return;
}

void set_on_start(qjs::Object cb) {
    state.on_start = cb.as<OnStart>();
}

void set_on_finish(qjs::Object cb) {
    state.on_finish = cb.as<OnFinish>();
}

void set_on_command(qjs::Object cb) {
    state.on_command = cb.as<OnCommand>();
}

void set_on_execution(qjs::Object cb) {
    state.on_execution = cb.as<OnExecution>();
}

}  // namespace catter::js
