#pragma once

#include <concepts>
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

#include "qjs.h"

namespace catter::js {

struct RuntimeConfig;

namespace detail {

template <typename T>
struct task_traits {
    constexpr static bool is_task = false;
};

template <typename T, typename E, typename C>
struct task_traits<kota::task<T, E, C>> {
    constexpr static bool is_task = true;
    using value_type = T;
    using error_type = E;
    using cancel_type = C;
};

template <typename T>
concept async_task = task_traits<std::remove_cvref_t<T>>::is_task;

}  // namespace detail

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

    kota::task<> stop();

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
    std::shared_ptr<kota::event> stopped_event;
    std::size_t job_budget = 32;
    bool running = false;
    bool idle_started = false;
};

JsLoop& js_loop();

JsLoop* current_js_loop() noexcept;

namespace detail {
void schedule_js_task(kota::task<>&& task);
void wake_js_loop() noexcept;

template <typename T>
struct PromiseWaitResult {
    kota::event done;
    std::optional<std::expected<T, qjs::Exception>> result;
};

template <typename E>
std::string task_error_message(E&& error) {
    using U = std::remove_cvref_t<E>;
    if constexpr(std::is_same_v<U, std::string>) {
        return std::forward<E>(error);
    } else if constexpr(std::constructible_from<std::string, E&&>) {
        return std::string(std::forward<E>(error));
    } else if constexpr(requires(const U& value) { std::string{value.message()}; }) {
        return std::string{error.message()};
    } else if constexpr(requires(const U& value) { value.what(); }) {
        return std::string{error.what()};
    } else {
        return "Async task failed.";
    }
}

template <typename Task>
kota::task<> settle_promise_task(qjs::PromiseCapability cap, Task task) {
    using TaskType = std::remove_cvref_t<Task>;
    using Traits = task_traits<TaskType>;
    static_assert(Traits::is_task, "task_to_promise expects a kota::task<...>");

    using R = typename Traits::value_type;
    using E = typename Traits::error_type;
    using C = typename Traits::cancel_type;
    static_assert(std::is_void_v<C>, "task_to_promise does not support cancellation tasks yet");

    auto resolve = [&cap]<typename... Args>(Args&&... args) {
        cap.resolve(std::forward<Args>(args)...);
        wake_js_loop();
    };
    auto reject = [&cap](std::string message) {
        cap.reject(std::move(message));
        wake_js_loop();
    };

    try {
        if constexpr(std::is_void_v<E>) {
            if constexpr(std::is_void_v<R>) {
                co_await std::move(task);
                resolve();
            } else {
                auto value = co_await std::move(task);
                resolve(std::move(value));
            }
        } else {
            auto result = co_await std::move(task);
            if(!result) {
                reject(task_error_message(std::move(result).error()));
                co_return;
            }

            if constexpr(std::is_void_v<R>) {
                resolve();
            } else {
                resolve(std::move(result).value());
            }
        }
    } catch(const qjs::Exception& ex) {
        reject(ex.what());
    } catch(const std::exception& ex) {
        reject(ex.what());
    } catch(...) {
        reject("Unknown exception in async C++ function");
    }
    co_return;
}
}  // namespace detail

template <typename T = void>
kota::task<std::expected<T, qjs::Exception>> promise_to_task(qjs::Promise promise) {
    auto state = std::make_shared<detail::PromiseWaitResult<T>>();
    auto js_ctx = promise.context();
    auto* loop = &kota::event_loop::current();

    auto notify = [state, loop] {
        auto signal_done = [](auto state) -> kota::task<> {
            state->done.set();
            co_return;
        };
        loop->schedule(signal_done(state));
    };

    auto fulfill = qjs::Promise::ThenCallback::from(
        js_ctx,
        [state, notify]([[maybe_unused]] qjs::Parameters args) {
            if(state->result) {
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
            } catch(const qjs::Exception& ex) {
                state->result.emplace(std::unexpected(ex));
            } catch(const std::exception& ex) {
                state->result.emplace(std::unexpected(qjs::Exception(ex.what())));
            }
            notify();
        });
    auto reject = qjs::Promise::ThenCallback::from(js_ctx, [state, notify](qjs::Parameters args) {
        if(state->result) {
            return;
        }
        state->result.emplace(std::unexpected(qjs::Exception(qjs::format_rejection(args))));
        notify();
    });

    promise.then(fulfill, reject);
    js_loop().wake();

    co_await state->done.wait();

    if(!state->result) {
        throw qjs::Exception("Promise wait was resumed before the promise settled.");
    }
    co_return std::move(*state->result);
}

template <detail::async_task Task>
qjs::Promise task_to_promise(JSContext* ctx, Task task) {
    auto cap = qjs::Promise::create(ctx);
    auto promise = cap.promise;

    try {
        detail::schedule_js_task(detail::settle_promise_task(cap, std::move(task)));
    } catch(const qjs::Exception& ex) {
        cap.reject(ex.what());
        detail::wake_js_loop();
    } catch(const std::exception& ex) {
        cap.reject(ex.what());
        detail::wake_js_loop();
    } catch(...) {
        cap.reject("Unknown exception in async C++ function");
        detail::wake_js_loop();
    }

    return promise;
}

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

}  // namespace catter::js
