module;

#include "modules/prelude.h"

module catter:capture.runtime_driver;

import :session.handler;

namespace catter {

class RuntimeDriver {
public:
    virtual ~RuntimeDriver() = default;

    virtual std::string_view name() const noexcept = 0;
    /// Runs `build`, its commands decided by `handler`: how the build ended.
    virtual kota::task<kota::process::exit_status> execute(const Build& build,
                                                           Handler& handler) const = 0;
};

/// Runs the build, its commands captured by the hook.
const RuntimeDriver& inject_runtime_driver() noexcept;

/// Plays a recorded build (see replay.cppm) instead of running one: the build command is the
/// replay file.
const RuntimeDriver& replay_runtime_driver() noexcept;

}  // namespace catter
