#include "js/async.h"

#include <quickjs.h>

#include "js/js.h"

namespace catter::js {
namespace {
bool loop_started = false;

JsLoop*& js_loop_ptr() {
    static JsLoop* loop = nullptr;
    return loop;
}

kota::task<> run_loop_task(JsLoop& js_loop, qjs::Runtime& runtime, kota::event_loop& event_loop) {
    try {
        co_await js_loop.run(runtime, event_loop);
    } catch(...) {
        loop_started = false;
        throw;
    }

    loop_started = false;
}

template <typename Fn>
kota::task<> deferred(Fn fn) {
    fn();
    co_return;
}
}  // namespace

JsLoop::JsLoop(std::size_t job_budget) noexcept : job_budget(job_budget == 0 ? 1 : job_budget) {}

JsLoop::~JsLoop() {
    if(loop) {
        running = false;
        wake_event.set();
    }
    cleanup_for(loop);
}

kota::task<> JsLoop::run(qjs::Runtime& runtime, kota::event_loop& event_loop) {
    rt = &runtime;
    loop = &event_loop;
    idle = kota::idle::create(event_loop);
    relay.emplace(event_loop.create_relay());
    wake_event.reset();
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

    cleanup_for(loop);
}

void JsLoop::wake() {
    if(!loop) {
        return;
    }

    loop->schedule(deferred([this] { wake_event.set(); }));
}

void JsLoop::request_stop() {
    if(!loop) {
        return;
    }

    loop->schedule(deferred([this] {
        running = false;
        wake_event.set();
    }));
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

JsLoop& js_loop() {
    auto* loop = js_loop_ptr();
    if(!loop) {
        throw qjs::Exception("QuickJS async loop is not installed.");
    }
    return *loop;
}

JsLoop* current_js_loop() noexcept {
    return js_loop_ptr();
}

namespace detail {

void schedule_js_task(kota::task<>&& task) {
    if(current_js_loop()) {
        kota::event_loop::current().schedule(std::move(task));
        return;
    }

    kota::event_loop loop;
    loop.schedule(std::move(task));
    loop.run();
}

void wake_js_loop() noexcept {
    if(auto* loop = current_js_loop()) {
        try {
            loop->wake();
        } catch(...) {}
    }
}

}  // namespace detail

JsLoopScope::JsLoopScope(JsLoop& loop) : loop(&loop) {
    auto*& current = js_loop_ptr();
    if(current && current != &loop) {
        throw qjs::Exception("QuickJS async loop is already installed.");
    }
    current = &loop;
}

JsLoopScope::~JsLoopScope() {
    auto*& current = js_loop_ptr();
    if(current == loop) {
        current = nullptr;
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
}

kota::task<> async_init_qjs(const RuntimeConfig& config) {
    if(loop_started) {
        throw qjs::Exception("QuickJS async loop is already running.");
    }

    detail::reset_runtime(config);

    auto& loop = kota::event_loop::current();
    auto& js = js_loop();
    loop_started = true;
    loop.schedule(run_loop_task(js, detail::runtime(), loop));

    const qjs::Context& ctx = detail::runtime().context();
    detail::register_catter_module(ctx);

    co_await async_eval(detail::js_lib_source(),
                        "catter",
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    co_await async_eval("import * as catter from 'catter'; globalThis.__catter_mod = catter;",
                        "get-mod.js",
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);

    js_mod_object() = ctx.global_this()["__catter_mod"].as<qjs::Object>();
}

kota::task<> async_run_js_file(std::string_view content, const std::string filepath) {
    co_await async_eval(content, filepath.data(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
}

}  // namespace catter::js
