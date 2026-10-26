module;

#include "modules/prelude.h"

module catter:driver.app_runner;

import :driver.option;
import :session.handler;

namespace catter::driver {

/// Runs a session: the handlers `options` names after `builtin`, if any, decide on the commands
/// of the build it runs or replays. How the build ended, which catter ends as; a build the
/// handlers did not start ended with 0.
kota::process::exit_status run_session(std::unique_ptr<Handler> builtin,
                                       const SessionOptions& options);

}  // namespace catter::driver
