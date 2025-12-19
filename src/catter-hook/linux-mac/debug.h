#pragma once

#ifdef DEBUG
#include "util/crossplat.h"
#include <filesystem>
#include "util/log.h"
#include "util/crossplat.h"
#include "linux-mac/config.h"

const inline auto __just_for_init = []() {
    auto path = catter::util::get_catter_data_path() / catter::config::hook::LOG_PATH_REL;
    catter::log::init_logger("catter-hook", path, false);
    return 0;
}();

#define INFO(...) LOG_INFO(__VA_ARGS__)
#define WARN(...) LOG_WARN(__VA_ARGS__)
#define ERROR(...) LOG_ERROR(__VA_ARGS__)
#define PANIC(...) LOG_CRITICAL(__VA_ARGS__)

#endif

#ifndef DEBUG
#define INFO(...)
#define WARN(...)
#define ERROR(...)
#define PANIC(...)
#endif  // !DEBUG
