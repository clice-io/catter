#pragma once
#include <cstdio>
#include <stdexcept>
#include <format>

#include <eventide/common/functional.h>
#include <eventide/async/async.h>
#include <eventide/async/io/loop.h>

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

struct process_info {
    eventide::task<int64_t, eventide::error> wait_task;
    eventide::pipe stdout_pipe;
    eventide::pipe stderr_pipe;
};

using process_event = eventide::function<process_info(eventide::event_loop&)>;

inline process_event make_process_event(eventide::process::options& opts) {
    return [opts = std::move(opts)](eventide::event_loop& loop) -> process_info {
        auto spawn_ret = eventide::process::spawn(opts, loop);
        if(!spawn_ret) {
            throw std::runtime_error(
                std::format("process spawn failed: {}", spawn_ret.error().message()));
        }

        return {
            .wait_task =
                [](eventide::process proc) noexcept -> eventide::task<int64_t, eventide::error> {
                auto wait_ret = co_await proc.wait();
                if(!wait_ret) {
                    co_return eventide::outcome_error(wait_ret.error());
                }
                co_return wait_ret->status;
            }(std::move(spawn_ret->proc)),
            .stdout_pipe = std::move(spawn_ret->stdout_pipe),
            .stderr_pipe = std::move(spawn_ret->stderr_pipe),
        };
    };
}

inline data::process_result capture_process_result(process_event proc_event,
                                                   FILE* stdout_sink = stdout,
                                                   FILE* stderr_sink = stderr) {

    auto event = [&]() -> eventide::task<data::process_result> {
        auto& current_loop = eventide::event_loop::current();

        auto [wait_task, stdout_pipe, stderr_pipe] = proc_event(current_loop);
        util::PipeProxy stdout_proxy(std::move(stdout_pipe), stdout_sink, "stdout");
        util::PipeProxy stderr_proxy(std::move(stderr_pipe), stderr_sink, "stderr");
        auto stdout_task = stdout_proxy.monitor();
        auto stderr_task = stderr_proxy.monitor();
        current_loop.schedule(stdout_task);
        current_loop.schedule(stderr_task);

        auto wait_ret = co_await std::move(wait_task);
        stdout_proxy.stop();
        stderr_proxy.stop();

        stdout_task.result();
        stderr_task.result();

        if(!wait_ret) {
            throw std::runtime_error(
                std::format("process wait failed: {}", wait_ret.error().message()));
        }

        co_return data::process_result{
            .code = *wait_ret,
            .std_out = stdout_proxy.output(),
            .std_err = stderr_proxy.output(),
        };
    };

    return wait(event());
}

}  // namespace catter
