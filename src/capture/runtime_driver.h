#pragma once

#include <string_view>
#include <kota/async/runtime/task.h>

#include "session/data.h"
#include "session/handler.h"

namespace catter {

class RuntimeDriver {
public:
    virtual ~RuntimeDriver() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual kota::task<data::process_result> execute(const Build& build,
                                                     Handler& handler) const = 0;
};

/// Runs the build, its commands captured by the hook.
const RuntimeDriver& inject_runtime_driver() noexcept;

/// Plays a recorded build (see replay.h) instead of running one: the build command is the
/// replay file.
const RuntimeDriver& replay_runtime_driver() noexcept;

}  // namespace catter
