#pragma once
#include <uv.h>
#include <memory>
#include <system_error>
#include <vector>
#include <format>

namespace catter::rpc::uv {
inline uv_loop_t* default_loop() {
    struct deleter {
        void operator() (uv_loop_t* loop) const {
            uv_loop_close(loop);
        }
    };

    static std::unique_ptr<uv_loop_t, deleter> instance{uv_default_loop()};
    return instance.get();
}

inline int run_loop(uv_loop_t* loop, uv_run_mode mode = UV_RUN_DEFAULT) {
    return uv_run(loop, mode);
}

inline int run(uv_run_mode mode = UV_RUN_DEFAULT) {
    return uv_run(default_loop(), mode);
}

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
Base* ptr_cast(Derived* ptr) {
    static_assert("Invalid uv_safe_cast");
    return reinterpret_cast<Base*>(ptr);
}

template <typename Ivokable>
int spawn_process(uv_loop_t* loop,
                  Ivokable&& exit_cb,
                  std::string& executable,
                  std::vector<std::string>& args) {
    struct Data {
        uv_process_t process;
        uv_process_options_t options;
        uv_stdio_container_t child_stdio[3];
        Ivokable exit_cb;
    };

    auto data = new Data{
        .process = uv_process_t{},
        .options = uv_process_options_t{},
        .child_stdio =
            {
                                {.flags = UV_IGNORE},
                                {.flags = UV_INHERIT_FD, .data = {.fd = 1}},
                                {.flags = UV_INHERIT_FD, .data = {.fd = 2}},
                                },
        .exit_cb = std::forward<Ivokable>(exit_cb)
    };

    std::vector<char*> raw_args;
    raw_args.push_back(executable.data());
    for(auto& arg: args) {
        raw_args.push_back(arg.data());
    }
    raw_args.push_back(nullptr);
    data->process.data = data;

    data->options.exit_cb = [](uv_process_t* process, int64_t exit_status, int term_signal) {
        auto d = static_cast<Data*>(process->data);
        d->exit_cb(exit_status, term_signal);
        delete d;
    };

    data->options.file = executable.c_str();
    data->options.args = raw_args.data();
    data->options.flags = UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS | UV_PROCESS_WINDOWS_HIDE;
    data->options.stdio_count = 3;
    data->options.stdio = data->child_stdio;
    return uv_spawn(loop, &data->process, &data->options);
}

template <typename Alloc, typename Read>
struct ReadContext {
    Alloc alloc_cb;
    Read read_cb;
};

template <typename Alloc, typename Read>
int read_start(uv_stream_t* stream, ReadContext<Alloc, Read>* ctx) {
    stream->data = ctx;
    return uv_read_start(
        stream,
        [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
            auto* ctx = static_cast<ReadContext<Alloc, Read>*>(handle->data);
            ctx->alloc_cb(handle, suggested_size, buf);
        },
        [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
            auto* ctx = static_cast<ReadContext<Alloc, Read>*>(stream->data);
            ctx->read_cb(stream, nread, buf);
        });
}

namespace sync {

void write(uv_stream_t* stream, std::vector<char>& payload);

void read(uv_stream_t* stream, char* dst, size_t len);

int spawn_process(uv_loop_t* loop, std::string& executable, std::vector<std::string>& args);
}  // namespace sync
}  // namespace catter::rpc::uv
