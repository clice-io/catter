module;

#include "modules/prelude.h"

module catter:driver.app_runner;

import :driver.option;
import :session.handler;

namespace catter::driver {

/// Runs a session: the handlers `options` names after `builtin`, if any, decide on the commands
/// of the build it runs or replays.
kota::task<> async_run(std::unique_ptr<Handler> builtin, const SessionOptions& options);

}  // namespace catter::driver
