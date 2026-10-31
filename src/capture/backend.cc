module;

#include "support/prelude.h"

module catter;

import :capture.backend;

namespace catter {

std::span<const Backend* const> capture_backends() noexcept {
#ifdef CATTER_MAC
    const static std::array backends = {&hook_backend(), &wrapper_backend(), &eslogger_backend()};
#elifdef CATTER_LINUX
    const static std::array backends = {&hook_backend(), &wrapper_backend(), &ptrace_backend()};
#else
    const static std::array backends = {&hook_backend(), &wrapper_backend()};
#endif
    return backends;
}

}  // namespace catter
