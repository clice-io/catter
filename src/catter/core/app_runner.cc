#include "app_runner.h"

#include <exception>
#include <filesystem>
#include <utility>

#include "app_config.h"
#include "option.h"
#include "runtime_driver.h"
#include "js/js.h"

namespace catter::app {

struct RunContext {
    js::CatterConfig script_config;
    std::filesystem::path working_directory;
    const core::RuntimeDriver* driver;

    static RunContext make(const core::CatterConfig& config) {
        auto driver = config.mode->driver;
        return {
            .script_config =
                {
                                .scriptPath = config.script_path.value(),
                                .scriptArgs = config.script_args,
                                .buildSystemCommand = config.command.value(),
                                .buildSystemCommandCwd = config.working_dir->path.string(),
                                .runtime = driver->runtime(),
                                .options =
                        {
                            .log = config.log,
                            .stdioMode = config.stdio_mode.value(),
                        }, .execute = true,
                                },
            .working_directory = config.working_dir->path,
            .driver = driver,
        };
    }
};

kota::task<> async_run(const core::CatterConfig& config) {
    auto context = RunContext::make(config);

    js::RuntimeScope runtime;
    std::exception_ptr error;
    try {
        runtime.start({.pwd = context.working_directory});

        auto script_absolute_path =
            std::filesystem::absolute(context.script_config.scriptPath).lexically_normal().string();

        co_await js::run_script(load_script_content(context.script_config.scriptPath),
                                script_absolute_path);

        auto script_config = co_await js::on_start(context.script_config);

        if(script_config.execute) {
            auto process_result = co_await context.driver->execute(script_config);
            co_await js::on_finish(core::to_js_process_result(std::move(process_result)));
        }
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
