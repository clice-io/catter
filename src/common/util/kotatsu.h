#pragma once
#include <cassert>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <cpptrace/exceptions.hpp>
#include <kota/support/functional.h>
#include <kota/async/async.h>
#include <kota/async/runtime/when.h>

#include "data.h"
#include "pipe_proxy.h"
#include "config/ipc.h"

namespace catter {

inline auto& default_loop() noexcept {
    static kota::event_loop loop{};
    return loop;
}

template <typename Task>
auto wait(Task&& task) {
    default_loop().schedule(task);
    default_loop().run();
    return task.result();
}

struct process_info {
    kota::task<int64_t, kota::error> wait_task;
    kota::pipe stdout_pipe;
    kota::pipe stderr_pipe;
};

using process_event = kota::function<process_info(kota::event_loop&)>;

inline process_event make_process_event(kota::process::options& opts) {
    return [opts = std::move(opts)](kota::event_loop& loop) -> process_info {
        auto spawn_ret = kota::process::spawn(opts, loop);
        if(!spawn_ret) {
            throw cpptrace::runtime_error(
                std::format("process spawn failed: {}", spawn_ret.error().message()));
        }

        return {
            .wait_task = [](kota::process proc) noexcept -> kota::task<int64_t, kota::error> {
                auto wait_ret = co_await proc.wait();
                if(!wait_ret) {
                    co_return kota::outcome_error(wait_ret.error());
                }
                co_return wait_ret->status;
            }(std::move(spawn_ret->proc)),
            .stdout_pipe = std::move(spawn_ret->stdout_pipe),
            .stderr_pipe = std::move(spawn_ret->stderr_pipe),
        };
    };
}

inline kota::task<data::process_result> capture_process_result(process_event proc_event,
                                                               FILE* stdout_sink = stdout,
                                                               FILE* stderr_sink = stderr) {
    auto& current_loop = kota::event_loop::current();

    auto [wait_task, stdout_pipe, stderr_pipe] = proc_event(current_loop);
    util::PipeProxy stdout_proxy(std::move(stdout_pipe), stdout_sink, "stdout");
    util::PipeProxy stderr_proxy(std::move(stderr_pipe), stderr_sink, "stderr");

    auto ret = co_await kota::when_all{std::move(wait_task),
                                       stdout_proxy.monitor(),
                                       stderr_proxy.monitor()};

    if(!ret) {
        throw cpptrace::runtime_error(std::format("process wait failed: {}", ret.error().message()));
    }

    auto [code, _1, _2] = *ret;

    co_return data::process_result{
        .code = code,
        .std_out = stdout_proxy.output(),
        .std_err = stderr_proxy.output(),
    };
}

}  // namespace catter
