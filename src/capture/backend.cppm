module;

#include "modules/prelude.h"

module catter:capture.backend;

import :session.session;

namespace catter {

/// The backends that run a build, which `--capture` names: the first is the
/// default.
std::span<const Backend* const> capture_backends() noexcept;

/// Runs the build with the hook in each of its processes.
const Backend& hook_backend() noexcept;

/// Runs the build with shims, catter-proxy under the names of the compilers, linkers and
/// archivers it runs, each before the directory of its program on its PATH.
const Backend& wrapper_backend() noexcept;

#ifdef CATTER_MAC
/// Runs the build as it is, and reads its commands from what Endpoint Security
/// tells eslogger of its processes: it only observes them.
const Backend& eslogger_backend() noexcept;
#endif

#ifdef CATTER_LINUX
/// Runs the build traced with ptrace, by a thread of catter, which reads each
/// program a process of the build runs as it execs it, a static program's
/// included: it only observes them.
const Backend& ptrace_backend() noexcept;
#endif

/// Plays a recorded build (see replay.cc) instead of running one: the build
/// command is the replay file.
const Backend& replay_backend() noexcept;

}  // namespace catter
