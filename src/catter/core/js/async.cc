#include "js/async.h"

#include <quickjs.h>

#include "js/js.h"

namespace catter::js {
namespace {
bool loop_started = false;
JsLoop* active_js_loop = nullptr;

kota::task<> run_loop_task(JsLoop& js_loop, qjs::Runtime& runtime, kota::event_loop& event_loop) {
    try {
        co_await js_loop.run(runtime, event_loop);
    } catch(...) {
        loop_started = false;
        throw;
    }

    loop_started = false;
    co_return;
}

template <typename Fn>
kota::task<> deferred(Fn fn) {
    fn();
    co_return;
}
}  // namespace

JsLoop::JsLoop(std::size_t job_budget) :
    stopped_event(std::make_shared<kota::event>(true)),
    job_budget(job_budget == 0 ? 1 : job_budget) {}

JsLoop::~JsLoop() {
    auto stopped = stopped_event;
    if(loop) {
        running = false;
        wake_event.set();
    }
    cleanup_for(loop);
    stopped->set();
}

kota::task<> JsLoop::run(qjs::Runtime& runtime, kota::event_loop& event_loop) {
    rt = &runtime;
    loop = &event_loop;
    idle = kota::idle::create(event_loop);
    relay.emplace(event_loop.create_relay());
    wake_event.reset();
    stopped_event->reset();
    running = true;

    return run_impl();
}

kota::task<> JsLoop::run_impl() {
    while(running) {
        if(!JS_IsJobPending(rt->js_runtime())) {
            stop_idle();

            co_await wake_event.wait();
            wake_event.reset();
            continue;
        }

        if(!idle_started) {
            idle.start();
            idle_started = true;
        }

        co_await idle.wait();
        if(!running) {
            break;
        }

        drain_jobs_with_budget(*rt, job_budget);
    }

    auto stopped = stopped_event;
    cleanup_for(loop);
    stopped->set();
    co_return;
}

void JsLoop::wake() {
    if(!loop) {
        return;
    }

    loop->schedule(deferred([this] { wake_event.set(); }));
}

kota::task<> JsLoop::stop() {
    auto stopped = stopped_event;
    if(!loop) {
        co_return;
    }

    loop->schedule(deferred([this] {
        running = false;
        wake_event.set();
    }));
    co_await stopped->wait();
    co_return;
}

bool JsLoop::is_running() const noexcept {
    return running;
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

void JsLoop::stop_idle() noexcept {
    if(idle_started) {
        idle.stop();
        idle_started = false;
    }
}

namespace detail {

void schedule_js_task(kota::task<>&& task) {
    if(active_js_loop) {
        kota::event_loop::current().schedule(std::move(task));
        return;
    }

    kota::event_loop loop;
    loop.schedule(std::move(task));
    loop.run();
}

bool wake_js_loop() noexcept {
    if(!active_js_loop) {
        return false;
    }

    try {
        active_js_loop->wake();
    } catch(...) {
        return false;
    }
    return true;
}

}  // namespace detail

JsLoopScope::JsLoopScope(JsLoop& loop) : loop(&loop) {
    if(active_js_loop && active_js_loop != &loop) {
        throw qjs::Exception("QuickJS async loop is already installed.");
    }
    active_js_loop = &loop;
}

JsLoopScope::~JsLoopScope() {
    if(active_js_loop == loop) {
        active_js_loop = nullptr;
    }
}

kota::task<> async_eval(std::string_view input, const char* filename, int eval_flags) {
    auto& ctx = detail::runtime().context();
    auto eval_result = ctx.eval(input, filename, eval_flags);

    if(JS_IsPromise(eval_result.value())) {
        auto result = co_await promise_to_task(qjs::Promise::from_value(std::move(eval_result)));
        if(!result) {
            throw std::move(result.error());
        }
    }
    co_return;
}

kota::task<> async_init_qjs(const RuntimeConfig& config) {
    if(loop_started) {
        throw qjs::Exception("QuickJS async loop is already running.");
    }

    detail::reset_runtime(config);

    auto& loop = kota::event_loop::current();
    auto* js = active_js_loop;
    if(!js) {
        throw qjs::Exception("QuickJS async loop is not installed.");
    }
    loop_started = true;
    loop.schedule(run_loop_task(*js, detail::runtime(), loop));

    const qjs::Context& ctx = detail::runtime().context();
    detail::register_catter_module(ctx);

    co_await async_eval(detail::js_lib_source(),
                        "catter",
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    co_await async_eval("import * as catter from 'catter'; globalThis.__catter_mod = catter;",
                        "get-mod.js",
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);

    js_mod_object() = ctx.global_this()["__catter_mod"].as<qjs::Object>();
    co_return;
}

kota::task<> async_run_js_file(std::string_view content, const std::string filepath) {
    co_await async_eval(content, filepath.data(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    co_return;
}

}  // namespace catter::js
