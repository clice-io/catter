#pragma once
#include <stdexcept>
#include <format>

#include <eventide/async/loop.h>
#include <eventide/async/stream.h>
#include <eventide/async/task.h>
#include <eventide/async/process.h>

#include "config/ipc.h"
#include "data.h"

namespace catter {

inline auto& default_loop() noexcept {
    static eventide::event_loop loop{};
    return loop;
}

template <typename Task>
auto wait(Task&& task) {
    default_loop().schedule(task);
    default_loop().run();
    return task.result();
}

inline eventide::task<int64_t> spawn(const eventide::process::options& opts) {
    auto spawn_ret = eventide::process::spawn(opts, default_loop());
    if(!spawn_ret) {
        throw std::runtime_error(
            std::format("process spawn failed: {}", spawn_ret.error().message()));
    }
    auto ret = co_await spawn_ret->proc.wait();
    if(!ret) {
        throw std::runtime_error(std::format("process wait failed: {}", ret.error().message()));
    }
    co_return ret->status;
}

}  // namespace catter
