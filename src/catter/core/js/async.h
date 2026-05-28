#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
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

    bool is_stopped() const noexcept;

    template <typename T = void>
    kota::task<T, qjs::Error> promise_to_task(qjs::Promise promise) {
        assert(this->can_drive_jobs() && "QuickJS async loop is not running.");

        auto js_ctx = promise.context();

        struct WaitState {
            kota::event done_event{};
            std::optional<std::expected<T, qjs::Error>> result{};
        };

        // NOTE:
        // When `done_event.set()` is called, the state object is at risk of being destroyed
        // immediately, because the coroutine may complete and release the state. In other words,
        // `set()` triggers a self-destruction *during* its own execution, before the `set()` call
        // returns. To handle this safely, we use a shared_ptr to manage the state, ensuring that it
        // remains alive until the event is fully processed and the result is set, even if that
        // means it outlives the coroutine's current scope.
        auto state = std::make_shared<WaitState>();
        auto notify = [state](std::expected<T, qjs::Error>&& result) {
            assert(!state->done_event.is_set() &&
                   "Promise settled after task completion, this should not happen.");
            state->result.emplace(std::move(result));
            state->done_event.set();
        };

        auto fulfill =
            qjs::Promise::OnFulfilled<void>::from(js_ctx, [js_ctx, notify](qjs::Value value) {
                try {
                    if constexpr(std::is_void_v<T>) {
                        notify({});
                    } else {
                        notify({value.as<T>()});
                    }
                } catch(const std::exception& ex) {
                    notify(std::unexpected(
                        qjs::Error::internal_error(js_ctx,
                                                   "Exception in promise fulfillment handler: {}",
                                                   ex.what())));
                } catch(...) {
                    notify(std::unexpected(qjs::Error::internal_error(
                        js_ctx,
                        "Unknown exception in promise fulfillment handler")));
                }
                return;
            });

        auto reject =
            qjs::Promise::OnRejected<void>::from(js_ctx, [js_ctx, notify](qjs::Value reason) {
                try {
                    if(reason.is_undefined()) {
                        notify(std::unexpected(
                            qjs::Error::internal_error(js_ctx, "Promise rejected with undefined")));
                    } else if(reason.is_error()) {
                        notify(std::unexpected(reason.as<qjs::Error>()));
                    } else {
                        notify(std::unexpected(
                            qjs::Error::internal_error(js_ctx,
                                                       "Promise rejected with value: {}",
                                                       qjs::json::stringify(reason))));
                    }
                } catch(const std::exception& e) {
                    notify(std::unexpected(
                        qjs::Error::internal_error(js_ctx,
                                                   "Exception in promise rejection handler: {}",
                                                   e.what())));
                } catch(...) {
                    notify(std::unexpected(qjs::Error::internal_error(
                        js_ctx,
                        "Unknown exception in promise rejection handler")));
                }
                return;
            });
        // NOTE:
        // 1. When we call then on a promise, if the promise is already fulfilled or rejected
        // the corresponding callback will be called immediately. But the
        // current coroutine is not yet suspended at that time. Make sure the callbacks can handle
        // this situation.
        // 2. When this coroutine is destroyed, the capture data of the callbacks will also be
        // destroyed, but the callbacks may still be called by the promise, so we need to make sure
        // the callbacks can handle this situation.
        promise.then(fulfill, reject);

        this->wake();

        co_await state->done_event.wait();
        assert(state->result.has_value() && "Promise callbacks did not set the result.");

        if(!state->result->has_value()) {
            co_await kota::fail(state->result->error());
        }
        co_return state->result->value();
    }

    template <typename T>
    qjs::Promise task_to_promise(JSContext* ctx, kota::task<T, qjs::Error> task) {

        assert(this->can_drive_jobs() && "QuickJS async loop is not running.");

        auto cap = qjs::PromiseCapability::create(ctx);
        auto promise = cap.promise;

        try {
            this->schedule(this->settle_promise_task(cap, std::move(task)));
        } catch(const std::exception& ex) {
            cap.executor.reject(
                qjs::Error::internal_error(ctx, "Exception in async C++ function: {}", ex.what()));
            this->wake();
        } catch(...) {
            cap.executor.reject(
                qjs::Error::internal_error(ctx, "Unknown exception in async C++ function"));
            this->wake();
        }

        return promise;
    }

private:
    enum class RunState {
        stopped,
        running,
        stopping,
    };

    template <typename T>
    kota::task<> settle_promise_task(qjs::PromiseCapability cap, kota::task<T, qjs::Error> task) {

        try {
            auto result = co_await std::move(task);
            if(result.has_value()) {
                if constexpr(std::is_void_v<T>) {
                    cap.executor.resolve();
                } else {
                    cap.executor.resolve(std::move(result).value());
                }
            } else {
                cap.executor.reject(std::move(result).error());
            }
        } catch(const std::exception& ex) {
            cap.executor.reject(qjs::Error::internal_error(cap.promise.context(),
                                                           "Exception in async C++ function: {}",
                                                           ex.what()));
        } catch(...) {
            cap.executor.reject(
                qjs::Error::internal_error(cap.promise.context(),
                                           "Unknown exception in async C++ function"));
        }
        this->wake();
        co_return;
    }

    kota::task<> run_impl();

    void cleanup_for(kota::event_loop* owner) noexcept;

    void start_idle();

    void stop_idle() noexcept;

    bool can_drive_jobs() const noexcept;

    qjs::Runtime* rt = nullptr;
    kota::event_loop* loop = nullptr;
    kota::idle idle;
    std::optional<kota::relay> relay;
    std::shared_ptr<kota::event> stopped_event;
    std::size_t job_budget = 32;
    RunState run_state = RunState::stopped;
    bool idle_started = false;
};

}  // namespace catter::js
