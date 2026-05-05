#include "app_runner.h"

#include <exception>
#include <utility>

#include "app_config.h"
#include "option.h"
#include "runtime_driver.h"
#include "js/async.h"
#include "js/js.h"

namespace catter::app {

kota::task<> async_run(const core::CatterConfig& config) {
    auto context = core::RunContext(config);
    auto script_config = context.make_script_config();
    auto script_content = load_script_content(script_config.scriptPath);
    js::JsLoop js_loop;
    js::JsLoopScope js_loop_scope(js_loop);
    std::exception_ptr error;

    try {
        co_await js::async_init_qjs({.pwd = context.working_directory()});
        co_await js::async_run_js_file(script_content, script_config.scriptPath);

        script_config = co_await js::on_start(script_config);
        context.apply_option_defaults(script_config);

        if(script_config.execute) {
            auto process_result = co_await context.driver.execute(script_config);
            co_await js::on_finish(core::to_js_process_result(std::move(process_result)));
        }
    } catch(...) {
        error = std::current_exception();
    }

    if(error) {
        std::rethrow_exception(error);
    }
}

}  // namespace catter::app
