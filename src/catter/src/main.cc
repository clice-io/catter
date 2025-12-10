#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <uv.h>
#include <vector>
#include <string>
#include <print>

#include "librpc/data.h"
#include "libutil/uv.h"

#ifdef _WIN32
constexpr char PIPE_NAME[] = R"(\\.\pipe\catter)";
#else
constexpr char PIPE_NAME[] = "pipe-catter.sock";
#endif

using namespace catter;

template <typename Ivokable>
    requires std::invocable<Ivokable, int64_t, int>
uv::async::Lazy<int64_t> spawn_process(rpc::data::command& cmd) {
    std::vector<char*> args;
    args.push_back(cmd.executable.data());
    for(auto& arg: cmd.args) {
        args.push_back(arg.data());
    }
    args.push_back(nullptr);

    uv_process_options_t options;
    uv_stdio_container_t child_stdio[3] = {
                                {.flags = UV_IGNORE, .data = {}},
                                {.flags = UV_INHERIT_FD, .data = {.fd = 1}},
                                {.flags = UV_INHERIT_FD, .data = {.fd = 2}},
                                };

    options.file = cmd.executable.c_str();
    options.args = args.data();
    options.flags = UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS | UV_PROCESS_WINDOWS_HIDE;
    options.stdio_count = 3;
    options.stdio = child_stdio;
    
    co_return co_await uv::async::awaiter::Spwan(uv::default_loop(), &options);
}
uv::async::Lazy<void> accept(uv_stream_t* server) {
    auto client = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());
    uv_accept(server, uv::cast<uv_stream_t>(client));
    std::println("Client connected.");
    co_return;
}


uv::async::Lazy<void> loop() {
    auto server = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());
    uv_pipe_bind(server, PIPE_NAME);

    std::vector<uv::async::Lazy<void>> acceptors;
    server->data = &acceptors;
    uv_listen(uv::cast<uv_stream_t>(server), 128, [](uv_stream_t* server, int status) { 
        auto &acceptors = *static_cast<std::vector<uv::async::Lazy<void>>*>(server->data);
        acceptors.push_back(accept(server));
    });

    co_await std::suspend_always{}; // process events

    for (auto& acceptor: acceptors) {
        if (!acceptor.done()) {
            std::println("Error: pending acceptor on shutdown.");
        }
    }
    co_return;
}

uv::async::Lazy<void> foo(uv::async::Lazy<void>& loop_task) {
    auto pipe = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());

    auto status = co_await uv::async::awaiter::PipeConnect(pipe, PIPE_NAME);
    if(status != 0) {
        std::println("Connect failed: {}", uv_strerror(status));
        co_return;
    }

    std::println("Connected to pipe.");
    char buffer;
    while(true) {
        ssize_t nread =
            co_await uv::async::awaiter::Read(uv::cast<uv_stream_t>(pipe), &buffer, 1);
        if(nread < 0) {
            std::println("Read error: {}", uv_strerror(nread));
            break;
        } else {
            std::print("{}", buffer);
        }
    }

    loop_task.resume();
    co_return;
}




int main(int argc, char* argv[], char* envp[]) {
    auto loop_task = loop();

    auto foo_task = foo(loop_task);

    uv::run();
    try {
        loop_task.get();
        foo_task.get();
    } catch (std::exception& e) {
        std::println("Exception: {}", e.what());
    }
    return 0;
}
