module;

#include "modules/prelude.h"

module catter:capture.backend;

import :session.handler;
import :session.session;

namespace catter {

/// A way to capture a build: it starts the build, and reports each command the
/// build starts to the session.
class Backend {
public:
    virtual ~Backend() = default;

    /// Its name, which the handlers are told.
    virtual std::string_view name() const noexcept = 0;

    /// Runs `build`, its commands reported to `session`: how the build ended.
    virtual kota::task<kota::process::exit_status> run(const Build& build,
                                                       Session& session) const = 0;
};

/// The backends that run a build, which `--capture` names: the first is the
/// default.
std::span<const Backend* const> capture_backends() noexcept;

/// Runs the build with the hook in each of its processes.
const Backend& hook_backend() noexcept;

/// Plays a recorded build (see replay.cc) instead of running one: the build
/// command is the replay file.
const Backend& replay_backend() noexcept;

}  // namespace catter
