#include <iostream>
#include <print>
#include <string_view>

#include <kota/deco/deco.h>

#include "qjs.h"
#include "app_runner.h"
#include "util/crossplat.h"
#include "util/log.h"
#include "config/catter.h"
#include "option.h"

using namespace catter;

int main(int argc, char* argv[]) {
    auto args = kota::deco::util::argvify(argc, argv, 1);
    kota::deco::cli::text::set_default_renderer(kota::deco::cli::text::ModernRenderer());
    // -1 is continue, else return
    int ret = -1;

    try {
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
            return ret;
        }
        if(!res) {
            std::println("Error when parsing: \n{}\nUse -h or --help for usage",
                         res.error().message);
            return 1;
        }
        auto& options = res.value().options;
        app::run(options);
    } catch(const qjs::JSException& ex) {
        std::println("Eval JavaScript file failed: \n{}", ex.what());
        return 1;
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}
