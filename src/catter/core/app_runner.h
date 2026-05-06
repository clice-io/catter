#pragma once

#include <concepts>
#include <filesystem>
#include <string>
#include <utility>
#include <kota/async/runtime/task.h>

#include "js/async.h"
#include "js/js.h"

namespace catter::core {
struct CatterConfig;
}

namespace catter::app {

struct ScriptRunConfig {
    std::string script_content;
    std::string script_path;
    std::filesystem::path working_directory;
};

template <typename Function>
concept ScriptRunContinuation = requires(Function& continuation) {
    { continuation() } -> std::same_as<kota::task<>>;
};

template <ScriptRunContinuation Function>
kota::task<> async_run(ScriptRunConfig config, Function continuation) {
    js::JsLoop js_loop;
    js::JsLoopScope js_loop_scope(js_loop);

    try {
        co_await js::async_init_qjs({.pwd = std::move(config.working_directory)});
        co_await js::async_run_js_file(config.script_content, config.script_path);
        co_await continuation();
    } catch(...) {
        js_loop.request_stop();
        throw;
    }

    js_loop.request_stop();
    co_return;
}

inline kota::task<> async_run(ScriptRunConfig config) {
    co_await async_run(std::move(config), []() -> kota::task<> { co_return; });
    co_return;
}

kota::task<> async_run(const core::CatterConfig& config);

}  // namespace catter::app
