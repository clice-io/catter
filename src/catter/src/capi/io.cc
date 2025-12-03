#include <print>
#include "../apitool.h"

namespace {
CAPI(stdout_print, (const std::string content)->void) {
    std::print("{}", content);
}
}  // namespace
