module;

#include "modules/prelude.h"

module catter;

import :capture.backend;

namespace catter {

std::span<const Backend* const> capture_backends() noexcept {
#ifdef CATTER_WINDOWS
    const static std::array backends = {&hook_backend()};
#else
    const static std::array backends = {&hook_backend(), &wrapper_backend()};
#endif
    return backends;
}

}  // namespace catter
