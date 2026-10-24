#include <iostream>
#include <print>
#include <string_view>
#include <kota/async/io/loop.h>
#include <kota/deco/deco.h>

#include "app_runner.h"
#include "option.h"
#include "config/catter.h"
#include "js/qjs.h"
#include "util/crossplat.h"
#include "util/encoding.h"
#include "util/log.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace catter;

namespace {

int run(int argc, char* argv[]) {

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    auto args = kota::deco::util::argvify(argc, argv, 1);
    kota::deco::cli::text::set_default_renderer(kota::deco::cli::text::ModernRenderer());

    // -1 is continue, else return
    int ret = -1;

    try {
        try {
            log::init_logger("catter", util::get_catter_data_path() / config::core::LOG_PATH_REL);
        } catch(...) {
            // Without its log, catter reports what failed on the terminal only.
            log::mute_logger();
            throw;
        }
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
            std::println("{}\nUse -h or --help for usage", res.error().message);
            return 1;
        }
        kota::run(app::async_run(res->options));
    } catch(const qjs::JSException& ex) {
        std::println("JavaScript {}: {}", ex.js_error_name(), ex.message());
        std::println("JavaScript Stack trace:\n{}", ex.js_stack());
        // The native frames are for catter's developers: the log keeps them.
        LOG_ERROR("JavaScript {}: {}\nNative stack trace:\n{}",
                  ex.js_error_name(),
                  ex.message(),
                  ex.trace().to_string());
        return 1;
    } catch(const cpptrace::exception& ex) {
        std::println("{}", ex.message());
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
        return 1;
    } catch(const std::exception& ex) {
        std::println("{}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    util::Utf8Arguments arguments(argc, argv);
    return run(argc, arguments.argv());
}
#else
int main(int argc, char* argv[]) {
    return run(argc, argv);
}
#endif
