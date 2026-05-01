#pragma once

#include <cstddef>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <kota/async/async.h>

#include "js.h"

namespace catter::js {

class JsLoop {
public:
    explicit JsLoop(std::size_t job_budget = 64) noexcept;

    JsLoop(const JsLoop&) = delete;
    JsLoop& operator= (const JsLoop&) = delete;

    JsLoop(JsLoop&&) = delete;
    JsLoop& operator= (JsLoop&&) = delete;

    ~JsLoop();

    kota::task<> run(qjs::Runtime& runtime,
                     kota::event_loop& event_loop = kota::event_loop::current());

    void wake();

    void request_stop();

    bool is_running() const noexcept;

private:
    kota::task<> run_impl();

    void cleanup_for(kota::event_loop* owner) noexcept;

    void stop_idle() noexcept;

    qjs::Runtime* rt = nullptr;
    kota::event_loop* loop = nullptr;
    kota::idle idle;
    std::optional<kota::relay> relay;
    kota::event wake_event;
    std::size_t job_budget = 32;
    bool running = false;
    bool idle_started = false;
};

JsLoop& js_loop();

class JsLoopScope {
public:
    explicit JsLoopScope(JsLoop& loop);
    ~JsLoopScope();

    JsLoopScope(const JsLoopScope&) = delete;
    JsLoopScope& operator= (const JsLoopScope&) = delete;

    JsLoopScope(JsLoopScope&&) = delete;
    JsLoopScope& operator= (JsLoopScope&&) = delete;

private:
    JsLoop* loop = nullptr;
};

kota::task<> async_init_qjs(const RuntimeConfig& config);

kota::task<> async_eval(std::string_view input, const char* filename, int eval_flags);

kota::task<> async_run_js_file(std::string_view content, const std::string filepath);

namespace detail {

template <typename Fn>
kota::task<> defer(Fn fn) {
    fn();
    co_return;
}

}  // namespace detail

template <typename T = void>
kota::task<std::expected<T, qjs::Exception>> async_wait_for_promise(qjs::Promise promise) {
    enum class EvalState {
        pending,
        fulfilled,
        rejected,
    };

    struct EvalResult {
        EvalState state = EvalState::pending;
        kota::event done;
        std::optional<std::expected<T, qjs::Exception>> result;
    };

    auto state = std::make_shared<EvalResult>();
    auto js_ctx = promise.context();
    auto* loop = &kota::event_loop::current();

    auto notify = [state, loop] {
        loop->schedule(detail::defer([state] { state->done.set(); }));
    };

    auto fulfill = qjs::Promise::ThenCallback::from(
        js_ctx,
        [state, notify]([[maybe_unused]] qjs::Parameters args) {
            if(state->state != EvalState::pending) {
                return;
            }
            try {
                if constexpr(std::is_void_v<T>) {
                    state->result.emplace();
                } else {
                    if(args.empty()) {
                        state->result.emplace(
                            std::unexpected(qjs::Exception("Promise fulfilled without a value.")));
                    } else {
                        state->result.emplace(args[0].as<T>());
                    }
                }
                state->state = EvalState::fulfilled;
            } catch(const qjs::Exception& ex) {
                state->state = EvalState::rejected;
                state->result.emplace(std::unexpected(ex));
            } catch(const std::exception& ex) {
                state->state = EvalState::rejected;
                state->result.emplace(std::unexpected(qjs::Exception(ex.what())));
            }
            notify();
        });
    auto reject = qjs::Promise::ThenCallback::from(js_ctx, [state, notify](qjs::Parameters args) {
        if(state->state != EvalState::pending) {
            return;
        }
        state->state = EvalState::rejected;
        state->result.emplace(std::unexpected(qjs::Exception(detail::format_rejection(args))));
        notify();
    });

    promise.then(fulfill, reject);
    js_loop().wake();

    co_await state->done.wait();

    switch(state->state) {
        case EvalState::pending:
            throw qjs::Exception("Promise wait was resumed before the promise settled.");
        default:
            if(!state->result) {
                throw qjs::Exception("Promise settled without storing a result.");
            }
            co_return std::move(*state->result);
    }
}

}  // namespace catter::js
