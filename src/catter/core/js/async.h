#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <kota/async/async.h>

#include "qjs.h"

namespace catter::js {

class JsLoop {
public:
    explicit JsLoop(std::size_t job_budget = 64);

    JsLoop(const JsLoop&) = delete;
    JsLoop& operator= (const JsLoop&) = delete;

    JsLoop(JsLoop&&) = delete;
    JsLoop& operator= (JsLoop&&) = delete;

    ~JsLoop();

    kota::task<> run(qjs::Runtime& runtime,
                     kota::event_loop& event_loop = kota::event_loop::current());

    void wake();

    void schedule(kota::task<>&& task);

    kota::task<> stop();

    bool is_running() const noexcept;

private:
    kota::task<> run_impl();

    void cleanup_for(kota::event_loop* owner) noexcept;

    void start_idle();

    void stop_idle() noexcept;

    qjs::Runtime* rt = nullptr;
    kota::event_loop* loop = nullptr;
    kota::idle idle;
    std::optional<kota::relay> relay;
    std::shared_ptr<kota::event> stopped_event;
    std::size_t job_budget = 32;
    bool running = false;
    bool idle_started = false;
};

bool drain_jobs_with_budget(qjs::Runtime& runtime, std::size_t max_jobs = 64);

qjs::PromiseTaskBridge promise_task_bridge(JsLoop& js_loop) noexcept;

}  // namespace catter::js
