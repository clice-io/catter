#include "js/async.h"

#include <cassert>
#include <utility>
#include <quickjs.h>

#include "util/guard.h"

namespace catter::js {
namespace {
kota::task<> signal_event(std::shared_ptr<kota::event> event) {
    event->set();
    co_return;
}
}  // namespace

JsLoop::JsLoop(std::size_t job_budget) :
    stopped_event(std::make_shared<kota::event>(true)),
    job_budget(job_budget == 0 ? 1 : job_budget) {}

JsLoop::~JsLoop() {
    assert(!loop && "JsLoop must be stopped before destruction.");
}

kota::task<> JsLoop::run(qjs::Runtime& runtime, kota::event_loop& event_loop) {
    rt = &runtime;
    loop = &event_loop;
    idle = kota::idle::create(event_loop);
    relay.emplace(event_loop.create_relay());
    stopped_event = std::make_shared<kota::event>();
    running = true;

    return run_impl();
}

kota::task<> JsLoop::run_impl() {
    auto* owner = loop;
    auto stopped = stopped_event;
    auto finish = util::make_guard([this, owner, stopped] noexcept {
        cleanup_for(owner);
        if(owner) {
            owner->schedule(signal_event(stopped));
        } else {
            stopped->set();
        }
    });

    while(running) {
        if(JS_IsJobPending(rt->js_runtime())) {
            start_idle();
        } else {
            stop_idle();
        }

        co_await idle.wait();
        if(!running) {
            break;
        }

        drain_jobs_with_budget(*rt, job_budget);
    }

    co_return;
}

void JsLoop::wake() {
    if(!loop) {
        return;
    }

    start_idle();
}

void JsLoop::schedule(kota::task<>&& task) {
    if(loop) {
        loop->schedule(std::move(task));
        return;
    }

    kota::event_loop::current().schedule(std::move(task));
}

kota::task<> JsLoop::stop() {
    auto stopped = stopped_event;
    if(!loop) {
        co_return;
    }

    running = false;
    start_idle();
    co_await stopped->wait();
    co_return;
}

bool JsLoop::is_running() const noexcept {
    return running;
}

qjs::PromiseTaskBridge promise_task_bridge(JsLoop& js_loop) noexcept {
    return qjs::PromiseTaskBridge{
        .data = &js_loop,
        .schedule_task_fn =
            [](void* data, kota::task<>&& task) {
                assert(data);
                static_cast<JsLoop*>(data)->schedule(std::move(task));
            },
        .wake_jobs_fn =
            [](void* data) noexcept {
                assert(data);
                static_cast<JsLoop*>(data)->wake();
                return true;
            },
    };
}

bool drain_jobs_with_budget(qjs::Runtime& runtime, std::size_t max_jobs) {
    JSRuntime* js_rt = runtime.js_runtime();
    bool ran = false;
    JSContext* job_ctx = nullptr;

    for(std::size_t i = 0; i < max_jobs && JS_IsJobPending(js_rt); ++i) {
        int ret = JS_ExecutePendingJob(js_rt, &job_ctx);
        if(ret < 0) {
            if(job_ctx) {
                throw qjs::JSException::dump(job_ctx);
            }
            throw qjs::Exception("Error while executing pending JS job.");
        }
        if(ret == 0) {
            break;
        }
        ran = true;
    }

    return ran;
}

void JsLoop::cleanup_for(kota::event_loop* owner) noexcept {
    if(!owner || loop != owner) {
        return;
    }

    stop_idle();
    idle = {};
    relay.reset();
    running = false;
    rt = nullptr;
    loop = nullptr;
}

void JsLoop::start_idle() {
    if(!idle_started) {
        idle.start();
        idle_started = true;
    }
}

void JsLoop::stop_idle() noexcept {
    if(idle_started) {
        idle.stop();
        idle_started = false;
    }
}

}  // namespace catter::js
