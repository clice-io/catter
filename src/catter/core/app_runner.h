#pragma once

#include <filesystem>
#include <string>
#include <kota/async/runtime/task.h>

namespace catter::core {
struct CatterConfig;
}

namespace catter::app {

kota::task<> async_run(const core::CatterConfig& config);

}  // namespace catter::app
