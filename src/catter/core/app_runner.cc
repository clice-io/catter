#include "app_runner.h"

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

    co_await async_run(
        ScriptRunConfig{
            .script_content = std::move(script_content),
            .script_path = script_config.scriptPath,
            .working_directory = context.working_directory(),
        },
        [&]() -> kota::task<> {
            script_config = co_await js::on_start(script_config);
            context.apply_option_defaults(script_config);

            if(script_config.execute) {
                auto process_result = co_await context.driver.execute(script_config);
                co_await js::on_finish(core::to_js_process_result(std::move(process_result)));
            }
            co_return;
        });
    co_return;
}

}  // namespace catter::app
