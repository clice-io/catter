#pragma once
#include <coroutine>
#include <thread>
#include <utility>

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
    void return_value(const Ret& v) noexcept {
        this->result = v;
    }

    void return_value(Ret&& v) noexcept {
        this->result = std::move(v);
    }

    Ret&& result_rvalue() noexcept {
        return std::move(this->result);
    }

private:
    Ret result{};
};

template <>
class PromiseRet<void> {
public:
    void return_void() noexcept {}

    void result_rvalue() noexcept {}
};

template <typename Ret>
class LazyTask {
public:
    class Promise;

    using promise_type = Promise;
    using handle_type = std::coroutine_handle<promise_type>;

    LazyTask() = default;

    LazyTask(handle_type h) : handle{h} {}

    LazyTask(const LazyTask&) = delete;

    LazyTask(LazyTask&& other) noexcept : handle{std::exchange(other.handle, nullptr)} {}

    LazyTask& operator= (const LazyTask&) = delete;

    LazyTask& operator= (LazyTask&& other) noexcept {
        if(this != &other) {
            this->~LazyTask();
            new (this) LazyTask(std::move(other));
        }
        return *this;
    }

    ~LazyTask() {
        if(this->handle) {
            this->wait();
            this->handle.destroy();
        }
    }

    class Promise : public PromiseRet<Ret> {
    public:
        LazyTask get_return_object() noexcept {
            return {handle_type::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept {
            return {};
        }

        awaiter::final final_suspend() noexcept {
            return {.continues = this->previous};
        }

        void unhandled_exception() noexcept {}

        void set_previous(std::coroutine_handle<> h) noexcept {
            this->previous = h;
        }

    protected:
        std::coroutine_handle<> previous{std::noop_coroutine()};
    };

    struct awaiter {
        bool await_ready() noexcept {
            return false;
        }

        Ret await_resume() noexcept {
            return this->coro.promise().result_rvalue();
        }

        auto await_suspend(std::coroutine_handle<> h) noexcept {
            this->coro.promise().set_previous(h);
            return this->coro;
        }

        std::coroutine_handle<promise_type> coro;
    };

    awaiter operator co_await() noexcept {
        return {this->handle};
    }

    bool done() const noexcept {
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
        while(!this->handle.done()) {
            std::this_thread::yield();
        }
    }

    Ret get() noexcept {
        this->wait();
        return this->handle.promise().result_rvalue();
    }

protected:
    handle_type handle{nullptr};
};

}  // namespace catter::coro
