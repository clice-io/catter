#include "support/log.h"

#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "support/config/log.h"

// every target have a single logger instance
// which have commandline and file sink

namespace {
std::shared_ptr<spdlog::logger> logger_instance = nullptr;

/// Appends to the log file at a path. spdlog's file sinks take a narrow file
/// name, which Windows reads in the ANSI code page: a data directory past it,
/// as under a user name past ASCII, would not open.
class path_file_sink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit path_file_sink(const std::filesystem::path& path) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        file.open(path, std::ios::binary | std::ios::app);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        file.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    }

    void flush_() override {
        file.flush();
    }

private:
    std::ofstream file;
};
}  // namespace

namespace catter::log {

void init_logger(const std::string& logger_name,
                 const std::filesystem::path& file_path,
                 bool cmdline) noexcept {
    // add file sink and console sink
    auto file_sink = std::make_shared<path_file_sink>(file_path);
    file_sink->set_pattern(catter::config::log::LOG_PATTERN_FILE);
    std::vector<spdlog::sink_ptr> sinks{file_sink};
    if(cmdline) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(catter::config::log::LOG_PATTERN_CONSOLE);
        sinks.push_back(console_sink);
    }
    logger_instance = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
    spdlog::set_default_logger(logger_instance);
}

void mute_logger() noexcept {
    ::spdlog::set_level(::spdlog::level::off);
}

}  // namespace catter::log
