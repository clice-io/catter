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

template <typename Ret>
class Lazy {
public:
    class Promise;

    using promise_type = Promise;
    using handle_type = std::coroutine_handle<promise_type>;

    Lazy() = default;

    Lazy(handle_type h) : handle{h} {}

    Lazy(const Lazy&) = delete;

    Lazy(Lazy&& other) noexcept : handle{std::exchange(other.handle, nullptr)} {}

    Lazy& operator= (const Lazy&) = delete;

    Lazy& operator= (Lazy&& other) noexcept {
        if(this != &other) {
            this->~Lazy();
            new (this) Lazy(std::move(other));
        }
        return *this;
    }

    ~Lazy() {
        if(this->handle) {
            this->wait();
            this->handle.destroy();
        }
    }

    struct Promise : PromiseRet<Ret>, PromiseException, PromiseAwait {
        Lazy get_return_object() noexcept {
            return {handle_type::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept {
            return {};
        }
    };

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

    Ret get() {
        this->wait();
        this->handle.promise().rethrow_if_exception();
        return this->handle.promise().result_rvalue();
    }

protected:
    handle_type handle{nullptr};
};

}  // namespace catter::coro
