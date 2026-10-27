module;

#include "modules/prelude.h"

module catter;

import :capture.backend;

namespace catter {

std::span<const Backend* const> capture_backends() noexcept {
    const static std::array<const Backend*, 1> backends = {&hook_backend()};
    return backends;
}

}  // namespace catter
