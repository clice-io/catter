#pragma once
#include <format>
#include <cpptrace/exceptions.hpp>
#include <kota/async/async.h>

#include "session/data.h"

namespace catter {

/// Starts a program on the streams `opts` gives it; the task waits for its
/// exit. A coroutine passes `opts` as a variable of its own: a temporary one
/// in the expression it awaits makes MSVC give up the tail call of symmetric
/// transfer (error C4737 in release builds).
inline kota::task<data::process_result> run_process(const kota::process::options& opts) {
    auto spawned = kota::process::spawn(opts);
    if(!spawned) {
        throw cpptrace::runtime_error(std::format("Process `{}` spawn failed, because `{}`",
                                                  opts.file,
                                                  spawned.error().message()));
    }

    return [](kota::process process) -> kota::task<data::process_result> {
        auto exit = co_await process.wait();
        if(!exit) {
            throw cpptrace::runtime_error(
                std::format("Process wait failed `{}`", exit.error().message()));
        }
        co_return data::process_result{.code = exit->status};
    }(std::move(spawned->proc));
}

}  // namespace catter
