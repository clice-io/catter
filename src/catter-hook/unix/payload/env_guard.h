#pragma once

#include "unix/payload/session.h"
#include <vector>

namespace catter {

/// An RAII context to remove envs used by hook, make sure the execution of the target process will
/// not be affected by them.
class EnvGuard {
public:
    explicit EnvGuard(const char*** env_ptr) noexcept;
    ~EnvGuard() noexcept = default;

private:
    std::vector<char*> new_envs;
};
}  // namespace catter
