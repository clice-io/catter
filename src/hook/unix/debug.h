#pragma once

#ifdef DEBUG
#include "support/log.h"

#define INFO(...) LOG_INFO(__VA_ARGS__)
#define ERROR(...) LOG_ERROR(__VA_ARGS__)

#endif

#ifndef DEBUG
#define INFO(...)
#define ERROR(...)
#endif  // !DEBUG
