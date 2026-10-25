#include <algorithm>
#include <csignal>
#include <cstdio>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <cpptrace/exceptions.hpp>
#include <kota/async/io/loop.h>
#include <kota/deco/deco.h>

#include "driver/cli.h"
#include "driver/driver.h"
#include "support/config/catter.h"
#include "support/crossplat.h"
#include "support/encoding.h"
#include "support/log.h"

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

    try {
        try {
            log::init_logger("catter", get_catter_data_path() / config::CATTER_LOG_PATH);
        } catch(...) {
            // Without its log, catter reports what failed on the terminal only.
            log::mute_logger();
            throw;
        }
        if(!args.empty() && (args.front() == "-h" || args.front() == "--help")) {
            std::println("{}", driver::usage_of_catter());
            return 0;
        }
        if(args.empty() || args.front().starts_with('-')) {
            std::println("catter: give a command\n\n{}", driver::usage_of_catter());
            return 1;
        }
        auto entry =
            std::ranges::find(driver::command_entries(), args.front(), &driver::CommandEntry::name);
        if(entry == driver::command_entries().end()) {
            std::println("catter: no command `{}`\n\n{}", args.front(), driver::usage_of_catter());
            return 1;
        }
        return entry->run(std::span(args).subspan(1));
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
}

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    Utf8Arguments arguments(argc, argv);
    return run(argc, arguments.argv());
}
#else
int main(int argc, char* argv[]) {
    return run(argc, argv);
}
#endif
