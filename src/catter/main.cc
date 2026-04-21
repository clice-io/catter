#include <iostream>
#include <print>
#include <string_view>
#include <kota/deco/deco.h>

#include "app_runner.h"
#include "option.h"
#include "qjs.h"
#include "config/catter.h"
#include "util/crossplat.h"
#include "util/exception.h"
#include "util/log.h"

using namespace catter;

int main(int argc, char* argv[]) {
    auto args = kota::deco::util::argvify(argc, argv, 1);
    kota::deco::cli::text::set_default_renderer(kota::deco::cli::text::ModernRenderer());
    // -1 is continue, else return
    int ret = -1;

    cpptrace::try_catch(
        [&] {
            log::init_logger("catter", util::get_catter_data_path() / config::core::LOG_PATH_REL);
            auto cli = kota::deco::cli::command<core::CatterConfig>(
                "catter [options for catter] script [options for script] -- [build system command]");
            cli.after<&core::CatterConfig::script_path>([](auto& step) {
                   unsigned idx = step.next_cursor();
                   std::span<std::string> original_argv = step.original_argv();
                   while(idx < original_argv.size() && original_argv[idx] != "--") {
                       step.options().script_args.push_back(original_argv[idx++]);
                   }
                   return step.seek(idx);
               })
                .after<&core::CatterConfig::help>([&](auto& step) {
                    step.usage(std::cout);
                    ret = 0;
                    return step.stop();
                });
            auto res = cli.invoke(args);
            if(ret != -1) {
                return;
            }
            if(!res) {
                std::println("Error when parsing: \n{}\nUse -h or --help for usage",
                             res.error().message);
                ret = 1;
                return;
            }
            auto& options = res.value().options;
            app::run(options);
        },
        [&](const qjs::JSException& ex) {
            std::println("{}",
                         util::format_exception("Eval JavaScript file failed: \n{}", ex.what()));
            ret = 1;
        },
        [&](const std::exception& ex) {
            std::println("{}", util::format_exception("Fatal error: {}", ex.what()));
            ret = 1;
        },
        [&] {
            std::println("{}", util::format_exception("Unknown fatal error."));
            ret = 1;
        });
    return ret == -1 ? 0 : ret;
}
