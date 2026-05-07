#include <chrono>
#include <cstdint>

#include "../apitool.h"

namespace {

CAPI(time_unix_ms, ()->int64_t) {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

CAPI(time_unix_us, ()->int64_t) {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

CAPI(time_unix_seconds, ()->int64_t) {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

CAPI(time_monotonic_ms, ()->int64_t) {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

CAPI(time_monotonic_us, ()->int64_t) {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace
