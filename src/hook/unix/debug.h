#pragma once

#ifdef DEBUG
#include <filesystem>

#include "support/config/hook_unix.h"
#include "support/crossplat.h"
#include "support/log.h"

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
