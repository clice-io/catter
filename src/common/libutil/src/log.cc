#include "libutil/log.h"
#include "libconfig/log.h"
#include <memory>
#include <spdlog/logger.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

// every target have a single logger instance
// which have commandline and file sink

namespace {
std::shared_ptr<spdlog::logger> logger_instance = nullptr;
}  // namespace

namespace catter::log {

void init_logger(const std::string& logger_name,
                 const std::filesystem::path& file_path,
                 bool cmdline) noexcept {
    // add file sink and console sink
    // preserve a week
    auto daily_file_sink =
        std::make_shared<spdlog::sinks::daily_file_format_sink_mt>(file_path.string(),
                                                                   0,
                                                                   0,
                                                                   false,
                                                                   7);
    daily_file_sink->set_pattern(catter::config::log::LOG_PATTERN_FILE);
    std::vector<spdlog::sink_ptr> sinks{daily_file_sink};
    if(cmdline) {
        daily_file_sink->set_pattern(catter::config::log::LOG_PATTERN_CONSOLE);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);
    }
    logger_instance = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
    spdlog::set_default_logger(logger_instance);
}

}  // namespace catter::log
