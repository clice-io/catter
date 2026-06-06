#include "app_runner.h"

#include <exception>
#include <utility>

#include "app_config.h"
#include "option.h"
#include "runtime_driver.h"
#include "js/js.h"

namespace catter::app {

kota::task<> async_run(const core::CatterConfig& config) {
    auto context = core::RunContext(config);
    auto script_config = context.make_script_config();
    auto script_content = load_script_content(script_config.scriptPath);

    js::RuntimeScope runtime;

    std::exception_ptr error;
    try {
        co_await runtime.start({.pwd = context.working_directory()});
        co_await js::run_script(script_content, script_config.scriptPath);

        script_config = co_await js::on_start(script_config);
        context.apply_option_defaults(script_config);

        if(script_config.execute) {
            auto process_result = co_await context.driver.execute(script_config);
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
