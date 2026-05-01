#pragma once

#include <kota/async/runtime/task.h>

#include "./option.h"

namespace catter::app {

kota::task<> async_run(const core::CatterConfig& config);

}  // namespace catter::app
