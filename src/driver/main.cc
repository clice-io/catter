module;

#include "modules/prelude.h"

#include "support/log.h"
#include "support/paths.h"

#ifdef CATTER_WINDOWS
#include <windows.h>
#endif

module catter;

import :driver.cli;
import :session.process;

using namespace catter;

namespace {

/// Prints why catter failed, where it can: the failure may be its stdout's own,
/// as a pipe whose reader has ended fails a write.
void report(std::string_view message) noexcept {
    try {
        std::println("{}", message);
    } catch(const std::exception&) {}
}

int run(std::span<const std::string> argv) {
    prepare_descriptors();
#ifdef CATTER_WINDOWS
    SetConsoleOutputCP(CP_UTF8);
#endif
    // The build writes to catter's stdout directly: what catter prints during
    // the build keeps its place among the build's output.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::vector<std::string> args(argv.begin() + 1, argv.end());

    try {
        try {
            log::init_logger("catter", data_path("log/catter.log"));
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
        report(ex.message());
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
        return 1;
    } catch(const std::exception& ex) {
        report(ex.what());
        return 1;
    } catch(...) {
        report("Unknown fatal error.");
        return 1;
    }
}

}  // namespace

extern "C++" int main(int argc, char* argv[]) {
#ifdef CATTER_WINDOWS
    // main's argv is in the ANSI code page, which holds few characters past
    // ASCII.
    return run(kotatsu::sys::arguments());
#else
    return run(std::vector<std::string>(argv, argv + argc));
#endif
}
