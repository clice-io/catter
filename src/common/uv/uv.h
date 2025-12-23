#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
#include <uv.h>
#include <memory>
#include <variant>
#include <vector>
#include <print>
#include "util/lazy.h"
#include "util/meta.h"

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
struct is_base_of<uv_req_t, Derived> {
    constexpr static bool value = requires {
        { &Derived::data } -> std::convertible_to<void* Derived::*>;
        { &Derived::type } -> std::convertible_to<uv_req_type Derived::*>;
        // other uv_req_t members...
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
Base* cast(Derived* ptr) noexcept {
    return reinterpret_cast<Base*>(ptr);
}

template <typename Task>
inline auto wait(Task&& task) {
    while(!task.done()) {
        uv::run();
    }
    return task.get();
}

template<typename Invocable>
int listen(uv_stream_t* stream, int backlog, Invocable& cb) noexcept {
    stream->data = std::addressof(cb);
    return uv_listen(
        stream,
        backlog,
        [](uv_stream_t* server_stream, int status) {
            (*static_cast<Invocable*>(server_stream->data))(server_stream, status);
        });
}

inline int listen(uv_stream_t* stream, int backlog, uv_connection_cb cb) noexcept {
    return listen(stream, backlog, cb);
}


}  // namespace catter::uv

namespace catter::uv::async {

namespace awaiter {

template <typename Derived, typename Ret>
class Base {
public:
    bool await_ready() {
        std::swap(this->tmp_data, static_cast<Derived*>(this)->data());
        auto ret = static_cast<Derived*>(this)->init();
        if(ret < 0) {
            throw std::runtime_error(uv_strerror(ret));
        }
        return false;
    }

    Ret await_resume() noexcept {
        std::swap(this->tmp_data, static_cast<Derived*>(this)->data());
        return this->result;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        this->handle = h;
    }

public:
    int init() {
        std::terminate();
    }  // to be specialized

    void*& data() {
        std::terminate();
    }  // to be specialized

    void resume() noexcept {
        this->handle.resume();
    }

    Ret& get_result() noexcept {
        return this->result;
    }

private:
    Ret result{};
    void* tmp_data{this};
    std::coroutine_handle<> handle{nullptr};
};

class Close : public Base<Close, std::monostate> {
public:
    Close(uv_handle_t* handle) : handle{handle} {}

    int init() {
        uv_close(this->handle,
                 [](uv_handle_t* handle) { static_cast<Close*>(handle->data)->close_cb(); });
        return 0;
    }

    void*& data() {
        return this->handle->data;
    }

private:
    void close_cb() {
        this->resume();
    }

    uv_handle_t* handle{nullptr};
};

class Read : public Base<Read, ssize_t> {
public:
    Read(uv_stream_t* stream, char* dst, size_t len) : stream{stream}, dst{dst}, remaining{len} {}

    int init() {
        return uv_read_start(
            this->stream,
            [](uv_handle_t* handle, size_t /*suggested_size*/, uv_buf_t* buf) {
                static_cast<Read*>(handle->data)->alloc_cb(buf);
            },
            [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* /*buf*/) {
                static_cast<Read*>(stream->data)->read_cb(nread);
            });
    }

    void*& data() {
        return this->stream->data;
    }

private:
    void alloc_cb(uv_buf_t* buf) {
        buf->base = this->dst;
        buf->len = this->remaining;
    }

    void read_cb(ssize_t nread) {
        if(nread > 0) {
            this->dst += nread;
            this->remaining -= nread;
            this->get_result() += nread;
            if(this->remaining == 0) {
                uv_read_stop(this->stream);
                this->resume();
            }
        } else {
            this->get_result() = nread;
            uv_read_stop(this->stream);
            this->resume();
        }
    }

    uv_stream_t* stream{nullptr};
    char* dst{nullptr};
    size_t remaining{0};
};

class Write : public Base<Write, int> {
public:
    Write(uv_stream_t* stream, uv_buf_t* bufs, unsigned int nbufs) :
        stream{stream}, bufs{bufs}, nbufs{nbufs} {}

    int init() {
        return uv_write(
            &this->req,
            this->stream,
            this->bufs,
            this->nbufs,
            [](uv_write_t* req, int status) { static_cast<Write*>(req->data)->write_cb(status); });
    }

    void*& data() {
        return this->req.data;
    }

private:
    void write_cb(int status) {
        this->get_result() = status;
        this->resume();
    }

    uv_write_t req{};

    uv_stream_t* stream{nullptr};
    uv_buf_t* bufs{nullptr};
    unsigned int nbufs{0};
};

class PipeConnect : public Base<PipeConnect, int> {
public:
    PipeConnect(uv_pipe_t* pipe, const char* name) : pipe{pipe}, name{name} {}

    int init() {
        uv_pipe_connect(&this->req, this->pipe, this->name, [](uv_connect_t* req, int status) {
            static_cast<PipeConnect*>(req->data)->connect_cb(status);
        });
        return 0;
    }

    void*& data() {
        return this->req.data;
    }

private:
    void connect_cb(int status) {
        this->get_result() = status;
        this->resume();
    }

    uv_connect_t req{};
    uv_pipe_t* pipe{nullptr};
    const char* name{nullptr};
};

class TCPConnect : public Base<TCPConnect, int> {
public:
    TCPConnect(uv_tcp_t* tcp, const struct sockaddr* addr) : tcp{tcp}, addr{addr} {}
    int init() {
        return uv_tcp_connect(&this->req, this->tcp, this->addr, [](uv_connect_t* req, int status) {
            static_cast<TCPConnect*>(req->data)->connect_cb(status);
        });
    }
    void*& data() {
        return this->req.data;
    }
private:
    void connect_cb(int status) {
        this->get_result() = status;
        this->resume();
    }
    uv_connect_t req{};
    uv_tcp_t* tcp{nullptr};
    const struct sockaddr* addr{nullptr};
};


class Spawn : public Base<Spawn, int64_t> {
public:
    Spawn(uv_loop_t* loop, uv_process_t* process, uv_process_options_t* options) :
        loop{loop}, process{process}, options{options} {}

    int init() {
        this->options->exit_cb =
            [](uv_process_t* process, int64_t exit_status, int /*term_signal*/) {
                static_cast<Spawn*>(process->data)->exit_cb(exit_status);
            };
        return uv_spawn(this->loop, this->process, this->options);
    }

    void*& data() {
        return this->process->data;
    }

private:
    void exit_cb(int64_t exit_status) {
        this->get_result() = exit_status;
        this->resume();
    }

    uv_loop_t* loop{nullptr};
    uv_process_t* process{nullptr};
    uv_process_options_t* options{nullptr};
};

}  // namespace awaiter
template <typename Ret>
struct LazyPromise;

template <typename Ret>
class [[nodiscard]] Lazy : public coro::TaskBase<LazyPromise<Ret>> {
public:
    using Base = coro::TaskBase<LazyPromise<Ret>>;
    using handle_type = Base::handle_type;

    using Base::Base;

    Lazy(Lazy&& other) noexcept = default;

    Lazy& operator= (Lazy&& other) noexcept {
        if(this != &other) {
            this->~Lazy();
            new (this) Lazy(std::move(other));
        }
        return *this;
    }

    ~Lazy() {
        if(this->handle) {
            this->check_done();
        }
    }

    void check_done() noexcept {
        assert(this->done());
    }

    bool done() noexcept {
        return Base::done() && this->handle.promise().cleaner.done();
    }

    struct awaiter {
        bool await_ready() noexcept {
            return false;
        }

        Ret await_resume() {
            this->coro.promise().rethrow_if_exception();
            return this->coro.promise().result_rvalue();
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept {
            if(this->coro.done() && this->coro.promise().cleaner.done()) {
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
        this->check_done();
        this->promise().rethrow_if_exception();
        return this->promise().result_rvalue();
    }
};

template <typename Ret>
struct LazyPromise : coro::PromiseRet<Ret>, coro::PromiseException {
public:
    friend class Lazy<Ret>;

    Lazy<Ret> get_return_object() noexcept {
        return {Lazy<Ret>::handle_type::from_promise(*this)};
    }

    std::suspend_never initial_suspend() noexcept {
        return {};
    }

    coro::awaiter::final final_suspend() noexcept {
        return {.continues = this->cleaner.get_handle()};
    }

    void set_previous(std::coroutine_handle<> h) noexcept {
        this->cleaner.get_handle().promise().set_previous(h);
    }

    template <typename T>
        requires is_base_of_v<uv_handle_t, T>
    void register_handle_for_close(T* handle) noexcept {

        this->handles.push_back(
            std::shared_ptr<uv_handle_t>(uv::cast<uv_handle_t>(handle),
                                         [](uv_handle_t* h) { delete reinterpret_cast<T*>(h); }));
    }

private:
    static coro::Lazy<void> close_handles(std::vector<std::shared_ptr<uv_handle_t>>& handles) {
        co_await std::suspend_always{};
        for(auto& handle: handles) {
            co_await async::awaiter::Close(handle.get());
        }
        co_return;
    }

    std::vector<std::shared_ptr<uv_handle_t>> handles{};
    coro::Lazy<void> cleaner{close_handles(this->handles)};
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
        requires meta::is_specialization_of_v<Promise, LazyPromise>
    bool await_suspend(std::coroutine_handle<Promise> h) noexcept {
        h.promise().register_handle_for_close(this->ptr);
        return false;
    }

protected:
    T* ptr = new T{};
};

template <typename T>
    requires is_base_of_v<uv_handle_t, T>
struct Create : CreateBase<T> {};

template <>
struct Create<uv_pipe_t> : CreateBase<uv_pipe_t> {
    Create(uv_loop_t* loop, int ipc = 0) : CreateBase<uv_pipe_t>() {
        uv_pipe_init(loop, this->ptr, ipc);
    }
};

template <>
struct Create<uv_tcp_t> : CreateBase<uv_tcp_t> {
    Create(uv_loop_t* loop) : CreateBase<uv_tcp_t>() {
        uv_tcp_init(loop, this->ptr);
    }
};

template <typename... Vector>
    requires (std::is_same_v<std::remove_cvref_t<Vector>, std::vector<char>> && ...)
coro::Lazy<int> write(uv_stream_t* stream, Vector&&... vecs) {
    std::vector<uv_buf_t> bufs{uv_buf_init(vecs.data(), vecs.size())...};
    co_return co_await awaiter::Write(stream, bufs.data(), bufs.size());
}

inline coro::Lazy<ssize_t> read(uv_stream_t* stream, char* dst, size_t len) {
    co_return co_await awaiter::Read(stream, dst, len);
}

inline async::Lazy<int64_t> spawn(const std::string& exe_path,
                                  const std::vector<std::string>& args,
                                  bool close_stdio = false) {
    std::vector<const char*> line;
    line.emplace_back(exe_path.c_str());
    for(auto& arg: args) {
        line.push_back(arg.c_str());
    }
    line.push_back(nullptr);

    uv_process_options_t options{};
    uv_stdio_container_t child_stdio[3] = {
        {.flags = UV_IGNORE,     .data = {}       },
        {.flags = UV_INHERIT_FD, .data = {.fd = 1}},
        {.flags = UV_INHERIT_FD, .data = {.fd = 2}},
    };
    if(close_stdio) {
        child_stdio[1].flags = UV_IGNORE;
        child_stdio[2].flags = UV_IGNORE;
    }

    options.file = exe_path.c_str();
    options.args = const_cast<char**>(line.data());
    options.flags = UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS | UV_PROCESS_WINDOWS_HIDE;
    options.stdio_count = 3;
    options.stdio = child_stdio;

    auto process = co_await Create<uv_process_t>();

    co_return co_await uv::async::awaiter::Spawn(uv::default_loop(), process, &options);
}

}  // namespace catter::uv::async
