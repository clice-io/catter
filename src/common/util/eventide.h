#pragma once
#include <cstdio>
#include <stdexcept>
#include <format>

#include <eventide/async/async.h>

#include "config/ipc.h"
#include "data.h"
#include "pipe_proxy.h"

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

inline data::process_result
    capture_process_result(eventide::task<int64_t, eventide::error> wait_task,
                           eventide::pipe stdout_pipe = {},
                           eventide::pipe stderr_pipe = {},
                           FILE* stdout_sink = stdout,
                           FILE* stderr_sink = stderr) {
    util::PipeProxy stdout_proxy(std::move(stdout_pipe), stdout_sink, "stdout");
    util::PipeProxy stderr_proxy(std::move(stderr_pipe), stderr_sink, "stderr");
    auto stdout_task = stdout_proxy.monitor();
    auto stderr_task = stderr_proxy.monitor();

    auto event = [&]() -> eventide::task<int64_t> {
        auto wait_ret = co_await std::move(wait_task);
        stdout_proxy.stop();
        stderr_proxy.stop();

        stdout_task.result();
        stderr_task.result();

        if(!wait_ret) {
            throw std::runtime_error(
                std::format("process wait failed: {}", wait_ret.error().message()));
        }
        co_return *wait_ret;
    };

    auto task = event();

    default_loop().schedule(stdout_task);
    default_loop().schedule(stderr_task);
    default_loop().schedule(task);
    default_loop().run();

    return data::process_result{
        .code = task.result(),
        .std_out = stdout_proxy.output(),
        .std_err = stderr_proxy.output(),
    };
}

}  // namespace catter
