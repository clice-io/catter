#pragma once
#include <cassert>
#include <coroutine>
#include <thread>
#include <utility>
#include <exception>

namespace catter::coro {
namespace awaiter {
struct final {
    bool await_ready() noexcept {
        return false;
    }

    void await_resume() noexcept {}

    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
        return this->continues;
    }

    std::coroutine_handle<> continues;
};
}  // namespace awaiter

template <typename Ret>
class PromiseRet {
public:
    void return_value(const Ret& v) noexcept(std::is_nothrow_copy_constructible_v<Ret>) {
        this->result = v;
    }

    void return_value(Ret&& v) noexcept(std::is_nothrow_move_constructible_v<Ret>) {
        this->result = std::move(v);
    }

    Ret&& result_rvalue() noexcept {
        return std::move(this->result);
    }

protected:
    Ret result{};
};

template <>
class PromiseRet<void> {
public:
    void return_void() noexcept {}

    void result_rvalue() noexcept {}
};

class PromiseAwait {
public:
    awaiter::final final_suspend() noexcept {
        return {.continues = this->previous};
    }

    void set_previous(std::coroutine_handle<> h) noexcept {
        this->previous = h;
    }

protected:
    std::coroutine_handle<> previous{std::noop_coroutine()};
};

class PromiseException {
public:
    void unhandled_exception() noexcept {
        this->exception = std::current_exception();
    }

    void rethrow_if_exception() {
        if(this->exception) {
            std::rethrow_exception(this->exception);
        }
    }

protected:
    std::exception_ptr exception{nullptr};
};

template <typename Promise>
class [[nodiscard]] TaskBase {
public:
    using promise_type = Promise;
    using handle_type = std::coroutine_handle<promise_type>;
    TaskBase() = default;

    TaskBase(handle_type h) : handle{h} {}

    TaskBase(const TaskBase&) = delete;

    TaskBase(TaskBase&& other) noexcept : handle{std::exchange(other.handle, nullptr)} {}

    TaskBase& operator= (const TaskBase&) = delete;

    TaskBase& operator= (TaskBase&& other) noexcept {
        if(this != &other) {
            this->~TaskBase();
            new (this) TaskBase(std::move(other));
        }
        return *this;
    }

    ~TaskBase() {
        if(this->handle) {
            this->wait();
            this->handle.destroy();
        }
    }

    bool done() noexcept {
        return this->handle.done();
    }

    handle_type get_handle() noexcept {
        return this->handle;
    }

    handle_type release() noexcept {
        auto temp = this->handle;
        this->handle = nullptr;
        return temp;
    }

    void wait() noexcept {
        assert(!this->done());
    }

    void resume() noexcept {
        this->handle.resume();
    }

protected:
    promise_type& promise() noexcept {
        return this->handle.promise();
    }

    handle_type handle{nullptr};
};

template <typename Ret>
struct LazyPromise;

template <typename Ret>
class [[nodiscard]] Lazy : public TaskBase<LazyPromise<Ret>> {
public:
    using Base = TaskBase<LazyPromise<Ret>>;
    using promise_type = Base::promise_type;
    using handle_type = Base::handle_type;
    using Base::Base;

    struct awaiter {
        bool await_ready() noexcept {
            return false;
        }

        Ret await_resume() {
            this->coro.promise().rethrow_if_exception();
            return this->coro.promise().result_rvalue();
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept {
            if(this->coro.done()) {
                return false;
            } else {
                this->coro.promise().set_previous(h);
                return true;
            }
        }

        handle_type coro;
    };

    awaiter operator co_await() noexcept {
        return {this->handle};
    }

    Ret get() {
        this->wait();
        this->promise().rethrow_if_exception();
        return this->promise().result_rvalue();
    }
};

template <typename Ret>
struct LazyPromise : PromiseRet<Ret>, PromiseException, PromiseAwait {
    Lazy<Ret> get_return_object() noexcept {
        return {Lazy<Ret>::handle_type::from_promise(*this)};
    }

    std::suspend_never initial_suspend() noexcept {
        return {};
    }
};

}  // namespace catter::coro
