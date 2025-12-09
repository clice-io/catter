#include "libutil/uv.h"

namespace catter::uv {
uv_loop_t* default_loop() noexcept {
    struct deleter {
        void operator() (uv_loop_t* loop) const {
            uv_loop_close(loop);
        }
    };

    static std::unique_ptr<uv_loop_t, deleter> instance{uv_default_loop()};
    return instance.get();
}

int run_loop(uv_loop_t* loop, uv_run_mode mode) noexcept {
    return uv_run(loop, mode);
}

int run(uv_run_mode mode) noexcept {
    return uv_run(default_loop(), mode);
}
}  // namespace catter::uv

namespace catter::uv::async {}
