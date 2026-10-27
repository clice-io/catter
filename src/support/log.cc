#include "support/log.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <kota/fs/directory.h>
#include <kota/fs/file.h>
#include <kota/fs/path.h>

namespace {

/// Like `[2024-06-15 14:23:01.123] [info] [pid:12345][tid:123]`, then the
/// message on a line of its own.
#ifdef DEBUG
constexpr char PATTERN[] = "[%@] [%Y-%m-%d %H:%M:%S.%e] [%l] [pid:%P][tid:%t] \n %v";
#else
constexpr char PATTERN[] = "[%Y-%m-%d %H:%M:%S.%e] [%l] [pid:%P][tid:%t] \n %v";
#endif

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

void init_logger(const std::string& logger_name, std::string_view file_path, bool reopen) noexcept {
    auto sink = std::make_shared<path_file_sink>(file_path, reopen);
    sink->set_pattern(PATTERN);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(logger_name, std::move(sink)));
}

void mute_logger() noexcept {
    ::spdlog::set_level(::spdlog::level::off);
}

}  // namespace catter::log
