#pragma once

#include <memory>
#include <kota/async/runtime/task.h>

#include "driver/option.h"
#include "session/handler.h"

namespace catter::driver {

/// Runs a session: the handlers `options` names after `builtin`, if any, decide on the commands
/// of the build it runs or replays.
kota::task<> async_run(std::unique_ptr<Handler> builtin, const SessionOptions& options);

}  // namespace catter::driver
