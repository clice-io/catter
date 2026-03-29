#include <iostream>
#include <print>

#include <eventide/deco/runtime.h>

#include "opt/main/option.h"

#include "qjs.h"
#include "app_runner.h"
#include "util/crossplat.h"
#include "util/log.h"
#include "config/catter.h"

using namespace catter;

int main(int argc, char* argv[]) {
    auto args = deco::util::argvify(argc, argv, 1);

    try {
        log::init_logger("catter", util::get_catter_data_path() / config::core::LOG_PATH_REL);
        deco::cli::Dispatcher<core::Option> cli("catter [options] -- [build system command]");
        cli.dispatch(core::Option::HelpOpt::category_info,
                     [&]([[maybe_unused]] const core::Option& opt) { cli.usage(std::cout); })
            .dispatch(core::Option::CatterOption::category_info,
                      [&](const auto& opt) { app::run(opt.main_opt); })
            .dispatch([&](const auto&) { cli.usage(std::cout); })
            .when_err([&](const deco::cli::ParseError& err) {
                std::println("Error parsing options: {}", err.message);
                std::println("Use -h or --help for usage.");
            })
            .parse(args);
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
