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

inline eventide::task<eventide::process::wait_result> wait_and_stop(eventide::process& proc,
                                                                    util::PipeProxy& stdout_proxy,
                                                                    util::PipeProxy& stderr_proxy) {
    auto ret = co_await proc.wait();
    stdout_proxy.stop();
    stderr_proxy.stop();
    co_return ret;
}

inline data::process_result spawn_capture(const eventide::process::options& opts,
                                          FILE* stdout_sink = stdout,
                                          FILE* stderr_sink = stderr) {
    auto spawn_ret = eventide::process::spawn(opts, default_loop());
    if(!spawn_ret) {
        throw std::runtime_error(
            std::format("process spawn failed: {}", spawn_ret.error().message()));
    }

    util::PipeProxy stdout_proxy(std::move(spawn_ret->stdout_pipe), stdout_sink, "stdout");
    util::PipeProxy stderr_proxy(std::move(spawn_ret->stderr_pipe), stderr_sink, "stderr");

    auto stdout_task = stdout_proxy.monitor();
    auto stderr_task = stderr_proxy.monitor();
    auto wait_task = wait_and_stop(spawn_ret->proc, stdout_proxy, stderr_proxy);

    default_loop().schedule(stdout_task);
    default_loop().schedule(stderr_task);
    default_loop().schedule(wait_task);
    default_loop().run();

    stdout_task.result();
    stderr_task.result();

    auto wait_ret = wait_task.result();
    if(!wait_ret) {
        throw std::runtime_error(
            std::format("process wait failed: {}", wait_ret.error().message()));
    }

    return data::process_result{
        .code = wait_ret->status,
        .std_out = stdout_proxy.output(),
        .std_err = stderr_proxy.output(),
    };
}

}  // namespace catter
