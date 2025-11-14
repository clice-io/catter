#pragma once
#include <format>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <print>
#include <thread>

#include "hook/windows/env.h"

// Platform independent
class unique_file {
public:
    unique_file() {
        std::error_code ec;
        std::filesystem::create_directories(catter::win::capture_root, ec);
        if(ec) {
            std::println("Failed to create capture root directory: {}: {}",
                         catter::win::capture_root,
                         ec.message());
            return;
        }

        auto unique_id = std::hash<std::thread::id>()(std::this_thread::get_id());
        auto time_stamp = std::chrono::system_clock::now().time_since_epoch().count();

        auto path = std::format("{}/{}_{}", catter::win::capture_root, unique_id, time_stamp);
        this->ofs.open(path, std::ios::out | std::ios::app);

        if(!this->ofs.is_open()) {
            std::println("Failed to open output file: {}", path);
        }
    }

    template <typename... args_t>
    void writeln(std::format_string<args_t...> fmt, args_t&&... args) {
        std::lock_guard lock(this->mutex);
        if(this->ofs.is_open()) {
            this->ofs << std::format(fmt, std::forward<args_t>(args)...) << std::endl;
        }
    }

    ~unique_file() {
        this->ofs.close();
    }

private:
    std::ofstream ofs;
    std::mutex mutex;
};
