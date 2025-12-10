#pragma once
#include <uv.h>
#include <memory>
#include <vector>
#include <print>
#include "libutil/lazy_task.h"

namespace catter::uv {
uv_loop_t* default_loop() noexcept;
int run_loop(uv_loop_t* loop, uv_run_mode mode = UV_RUN_DEFAULT) noexcept;
int run(uv_run_mode mode = UV_RUN_DEFAULT) noexcept;

template <typename Base, typename Derived>
struct is_base_of {
    constexpr static bool value = false;
};

template <typename Derived>
struct is_base_of<uv_handle_t, Derived> {
    constexpr static bool value = requires {
        { &Derived::data } -> std::convertible_to<void* Derived::*>;
        { &Derived::loop } -> std::convertible_to<uv_loop_t * Derived::*>;
        { &Derived::type } -> std::convertible_to<uv_handle_type Derived::*>;
        // other uv_handle_t members...
    };
};

template <typename Derived>
struct is_base_of<uv_stream_t, Derived> {
    constexpr static bool value = is_base_of<uv_handle_t, Derived>::value && requires {
        { &Derived::write_queue_size } -> std::convertible_to<size_t Derived::*>;
        { &Derived::alloc_cb } -> std::convertible_to<uv_alloc_cb Derived::*>;
        { &Derived::read_cb } -> std::convertible_to<uv_read_cb Derived::*>;
        // other uv_stream_t members...
    };
};

template <typename Base, typename Derived>
constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

template <typename Base, typename Derived>
    requires is_base_of_v<Base, Derived>
Base* ptr_cast(Derived* ptr) noexcept {
    static_assert("Invalid uv_safe_cast");
    return reinterpret_cast<Base*>(ptr);
}

}  // namespace catter::uv

namespace catter::uv::async {

namespace awaiter {

class Read {
public:
    Read(uv_stream_t* stream, char* dst, size_t len) : stream{stream}, buf{dst}, remaining{len} {}

    bool await_ready() noexcept {
        this->tmp_data = this->stream->data;
        this->stream->data = this;

        auto ret = uv_read_start(
            this->stream,
            [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
                static_cast<Read*>(handle->data)->alloc_cb(handle, suggested_size, buf);
            },
            [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                static_cast<Read*>(stream->data)->read_cb(stream, nread, buf);
            });

        if(ret < 0) {
            this->nread = ret;
            return false;
        } else {
            return true;
        }
    }

    ssize_t await_resume() noexcept {
        this->stream->data = this->tmp_data;
        return this->nread;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        this->handle = h;
    }

private:
    void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        buf->base = this->buf;
        buf->len = this->remaining;
    }

    void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
        if(nread > 0) {
            this->buf += nread;
            this->remaining -= nread;
            this->nread += nread;
            if(this->remaining == 0) {
                uv_read_stop(this->stream);
                this->handle.resume();
            }
        } else {
            this->nread = nread;
            uv_read_stop(this->stream);
            this->handle.resume();
        }
    }

    uv_stream_t* stream{nullptr};
    std::coroutine_handle<> handle{};

    char* buf{nullptr};
    size_t remaining{0};

    void* tmp_data{nullptr};
    ssize_t nread{0};
};

class Write {
public:
    Write(uv_stream_t* stream, uv_buf_t* bufs, unsigned int nbufs) :
        stream{stream}, bufs{bufs}, nbufs{nbufs} {}

    bool await_ready() noexcept {
        this->req.data = this;
        auto ret = uv_write(&this->req,
                            this->stream,
                            this->bufs,
                            this->nbufs,
                            [](uv_write_t* req, int status) {
                                static_cast<Write*>(req->data)->write_cb(req, status);
                            });
        if(ret < 0) {
            this->status = ret;
            return false;
        } else {
            return true;
        }
    }

    int await_resume() noexcept {
        return this->status;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        this->handle = h;
    }

private:
    void write_cb(uv_write_t* req, int status) {
        this->status = status;
        this->handle.resume();
    }

    uv_stream_t* stream{nullptr};
    uv_buf_t* bufs{nullptr};
    unsigned int nbufs{0};

    int status{0};
    uv_write_t req{};
    std::coroutine_handle<> handle{};
};

class Close {
public:
    Close(uv_handle_t* handle) : uv_handle{handle} {}

    bool await_ready() noexcept {
        return false;
    }

    void await_resume() noexcept {}

    void await_suspend(std::coroutine_handle<> h) noexcept {
        this->uv_handle->data = this;
        this->handle = h;
        uv_close(this->uv_handle,
                 [](uv_handle_t* handle) { static_cast<Close*>(handle->data)->close_cb(handle); });
    }

private:
    void close_cb(uv_handle_t* handle) {
        this->handle.resume();
    }

    uv_handle_t* uv_handle{nullptr};
    std::coroutine_handle<> handle{};
};

class PipeConnect {
public:
    PipeConnect(uv_pipe_t* pipe, const char* name) : pipe{pipe}, name{name} {}

    bool await_ready() noexcept {
        return false;
    }

    int await_resume() noexcept {
        return this->status;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        this->handle = h;
        this->req.data = this;
        uv_pipe_connect(&this->req, this->pipe, this->name, [](uv_connect_t* req, int status) {
            static_cast<PipeConnect*>(req->data)->connect_cb(req, status);
        });
    }

private:
    void connect_cb(uv_connect_t* req, int status) {
        this->status = status;
        this->handle.resume();
    }

    uv_pipe_t* pipe{nullptr};
    const char* name{nullptr};

    int status{0};
    uv_connect_t req{};
    std::coroutine_handle<> handle{};
};

}  // namespace awaiter

template <typename Ret>
struct Lazy : catter::coro::Lazy<Ret> {
    using Base = catter::coro::Lazy<Ret>;
    using Base::Base;

    class promise_type : public Base::promise_type {
    public:
        Lazy get_return_object() noexcept {
            return {Base::handle_type::from_promise(*this)};
        }

        coro::awaiter::final final_suspend() noexcept {
            this->cleaner = close_handles(this->handles);
            this->cleaner.get_handle().promise().set_previous(this->previous);
            return {.continues = this->cleaner.get_handle()};
        }

        template <typename T>
            requires is_base_of_v<uv_handle_t, T>
        void register_handle_for_close(T* handle) noexcept {

            this->handles.push_back(
                std::shared_ptr<uv_handle_t>(uv::ptr_cast<uv_handle_t>(handle), [](uv_handle_t* h) {
                    delete reinterpret_cast<T*>(h);
                }));
        }

    private:
        static coro::Lazy<void> close_handles(std::vector<std::shared_ptr<uv_handle_t>>& handles) {
            co_await std::suspend_always{};
            for(auto& handle: handles) {
                co_await awaiter::Close(handle.get());
            }
            co_return;
        }

        std::vector<std::shared_ptr<uv_handle_t>> handles{};
        coro::Lazy<void> cleaner{};
    };
};

template <typename T>
    requires is_base_of_v<uv_handle_t, T>
class CreateBase {
public:
    CreateBase() = default;

    bool await_ready() noexcept {
        return false;
    }

    T* await_resume() noexcept {
        return this->ptr;
    }

    template <typename Promise>
    // requires std::is_base_of_v<async::Lazy<Ret>, Promise>
    // TODO
    bool await_suspend(std::coroutine_handle<Promise> h) noexcept {
        h.promise().template register_handle_for_close<T>(this->ptr);
        return false;
    }

protected:
    T* ptr = new T{};
};

template <typename T>
    requires is_base_of_v<uv_handle_t, T>
struct Create;

template <>
struct Create<uv_pipe_t> : CreateBase<uv_pipe_t> {
    Create(uv_loop_t* loop, int ipc = 0) : CreateBase<uv_pipe_t>() {
        uv_pipe_init(loop, this->ptr, ipc);
    }
};
}  // namespace catter::uv::async

namespace catter::uv::sync {
void write(uv_stream_t* stream, std::vector<char>& payload);

void read(uv_stream_t* stream, char* dst, size_t len);

int spawn_process(uv_loop_t* loop, std::string& executable, std::vector<std::string>& args);
}  // namespace catter::uv::sync
