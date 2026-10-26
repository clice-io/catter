#include "support/log.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <kota/fs/directory.h>
#include <kota/fs/file.h>
#include <kota/fs/path.h>

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
    path_file_sink(std::string_view path, bool reopen) : path(path), reopen(reopen) {
        kota::fs::create_directories(kota::fs::path::parent(path));
        // The log is there once the logger starts, though nothing is logged.
        auto opened = kota::fs::File::open(path, {.append = true, .create = true});
        if(opened && !reopen) {
            file = std::move(*opened);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        std::string_view text(formatted.data(), formatted.size());
        if(!reopen) {
            if(file) {
                file->write(text);
            }
        } else if(auto opened = kota::fs::File::open(path, {.append = true, .create = true})) {
            opened->write(text);
        }
    }

    void flush_() override {}

private:
    std::string path;
    bool reopen;
    std::optional<kota::fs::File> file;
};
}  // namespace

namespace catter::log {

void init_logger(const std::string& logger_name,
                 std::string_view file_path,
                 bool cmdline,
                 bool reopen) noexcept {
    // add file sink and console sink
    auto file_sink = std::make_shared<path_file_sink>(file_path, reopen);
    file_sink->set_pattern(catter::config::LOG_PATTERN_FILE);
    std::vector<spdlog::sink_ptr> sinks{file_sink};
    if(cmdline) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(catter::config::LOG_PATTERN_CONSOLE);
        sinks.push_back(console_sink);
    }
    logger_instance = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
    spdlog::set_default_logger(logger_instance);
}

void mute_logger() noexcept {
    ::spdlog::set_level(::spdlog::level::off);
}

}  // namespace catter::log
