#pragma once

#include <filesystem>
#include <string>
#include <kota/async/runtime/task.h>

namespace catter::driver {

struct CatterConfig;

kota::task<> async_run(const CatterConfig& config);

}  // namespace catter::driver
