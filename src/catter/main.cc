#include <csignal>
#include <cstdio>
#include <iostream>
#include <print>
#include <string_view>
#include <cpptrace/exceptions.hpp>
#include <kota/async/io/loop.h>
#include <kota/deco/deco.h>

#include "app_runner.h"
#include "option.h"
#include "config/catter.h"
#include "util/crossplat.h"
#include "util/encoding.h"
#include "util/log.h"

#ifdef _WIN32
#include <uv.h>
#include <windows.h>
#endif

using namespace catter;

namespace {

int run(int argc, char* argv[]) {

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    // libuv hands the build catter's stdio as duplicates of its own. The
    // standard handles themselves, inherited as well by every program catter
    // starts, would be held open by a server among them after catter exits.
    uv_disable_stdio_inheritance();
#else
    // A write to a handler that has exited fails, and is reported, instead of
    // killing catter. libuv resets the programs catter starts to the default.
    std::signal(SIGPIPE, SIG_IGN);
#endif
    // The build writes to catter's stdout directly: what catter prints during
    // the build keeps its place among the build's output.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

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
            "catter [options for catter] [script [options for script]] -- [build system command]");
        cli.after<&core::CatterConfig::script>([](auto& step) {
               unsigned idx = step.next_cursor();
               std::span<std::string> original_argv = step.original_argv();
               while(idx < original_argv.size() && original_argv[idx] != "--") {
                   step.options().script_args.push_back(original_argv[idx++]);
               }
               return step.seek(idx);
           })
            .after<&core::CatterConfig::handler>([](auto& step) {
                step.options().handlers.push_back({.connect = false, .text = step.value()});
                return step.next();
            })
            .after<&core::CatterConfig::connect>([](auto& step) {
                step.options().handlers.push_back({.connect = true, .text = step.value()});
                return step.next();
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
