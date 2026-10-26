#include <algorithm>
#include <cstdio>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/async/io/loop.h>
#include <kota/deco/deco.h>

#include "driver/cli.h"
#include "driver/driver.h"
#include "support/config/catter.h"
#include "support/log.h"
#include "support/paths.h"

#ifdef CATTER_WINDOWS
#include <windows.h>
#include <kota/sys/process.h>
#endif

using namespace catter;

namespace {

int run(std::span<const std::string> argv) {

#ifdef CATTER_WINDOWS
    SetConsoleOutputCP(CP_UTF8);
#endif
    // The build writes to catter's stdout directly: what catter prints during
    // the build keeps its place among the build's output.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::vector<std::string> args(argv.begin() + 1, argv.end());

    try {
        try {
            log::init_logger("catter", data_path(config::CATTER_LOG_PATH));
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
        auto entry = std::ranges::find(driver::subcommand_entries(),
                                       args.front(),
                                       &driver::SubcommandEntry::name);
        if(entry == driver::subcommand_entries().end()) {
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

int main(int argc, char* argv[]) {
#ifdef CATTER_WINDOWS
    // main's argv is in the ANSI code page, which holds few characters past
    // ASCII.
    return run(kota::sys::arguments());
#else
    return run(std::vector<std::string>(argv, argv + argc));
#endif
}
