#include "app_runner.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <utility>

#include "option.h"
#include "runtime_driver.h"
#include "builtin/builtin.h"
#include "js/js.h"
#include "js/script_handler.h"
#include "util/encoding.h"

namespace catter::app {

namespace {

std::string load_script_content(const std::string& script_path) {
    std::ifstream ifs{util::from_utf8(script_path)};
    if(!ifs.good()) {
        throw cpptrace::runtime_error(std::format("Failed to open script file `{}`", script_path));
    }

    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

/// Runs the session of `handler`: the build it starts, if any, and then its end.
kota::task<> run(core::Handler& handler, const core::CatterConfig& config) {
    auto build = co_await handler.on_start({
        .command = config.command.value(),
        .cwd = util::to_utf8(config.working_dir->path),
        .stdio_mode = config.stdio_mode.value(),
    });
    if(build) {
        auto result = co_await config.mode->driver->execute(*build, handler);
        co_await handler.on_finish(std::move(result));
    }
}

}  // namespace

kota::task<> async_run(const core::CatterConfig& config) {
    const auto& script_path = config.script_path.value();
    if(script_path.starts_with("script::")) {
        auto handler = builtin::make(script_path, config.script_args);
        if(!handler) {
            throw cpptrace::runtime_error(std::format("Unknown builtin script `{}`", script_path));
        }
        co_await run(*handler, config);
        co_return;
    }

    js::RuntimeScope runtime;
    std::exception_ptr error;
    try {
        runtime.start({.pwd = config.working_dir->path});

        auto script_absolute_path = util::to_utf8(
            std::filesystem::absolute(util::from_utf8(script_path)).lexically_normal());
        co_await js::run_script(load_script_content(script_absolute_path), script_absolute_path);

        js::ScriptHandler handler(script_path, config.script_args, config.log);
        co_await run(handler, config);
    } catch(...) {
        error = std::current_exception();
    }

    co_await runtime.stop();

    if(error) {
        std::rethrow_exception(error);
    }
    co_return;
}

}  // namespace catter::app
