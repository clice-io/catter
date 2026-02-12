#pragma once

#include <list>
#include <string>
#include <vector>

namespace catter {

/// An RAII context to remove envs used by hook, make sure the execution of the target process will
/// not be affected by them.
class EnvGuard {
public:
    explicit EnvGuard(const char*** env_ptr) noexcept;
    ~EnvGuard() noexcept = default;

    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator= (const EnvGuard&) = delete;
    EnvGuard(EnvGuard&&) = delete;
    EnvGuard& operator= (EnvGuard&&) = delete;

private:
    std::vector<char*> new_envs_;
    std::list<std::string> new_preload_;
};
}  // namespace catter
