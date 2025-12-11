#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <uv.h>
#include <vector>
#include <string>
#include <print>

#include "libconfig/rpc.h"
#include "libutil/lazy.h"
#include "libutil/uv.h"
#include "libutil/rpc_data.h"

using namespace catter;

uv::async::Lazy<void> accept(uv_stream_t* server) {
    auto client = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());
    uv_accept(server, uv::cast<uv_stream_t>(client));

    auto reader = [&](char* dst, size_t len) -> coro::Lazy<void> {
        auto ret = co_await uv::async::read(uv::cast<uv_stream_t>(client), dst, len);
        if(ret < 0) {
            throw std::runtime_error(uv_strerror(ret));
            co_return;
        }
    };

    try {
        while(true) {
            rpc::data::Request req = co_await Serde<rpc::data::Request>::co_deserialize(reader);
            switch(req) {
                case rpc::data::Request::MAKE_DECISION: {
                    std::println("Received MAKE_DECISION request.");
                    rpc::data::command_id_t parent_id =
                        co_await Serde<rpc::data::command_id_t>::co_deserialize(reader);
                    std::println("Parent ID: {}", parent_id);

                    rpc::data::command cmd =
                        co_await Serde<rpc::data::command>::co_deserialize(reader);

                    std::println("Spawning process: {}", cmd.executable);
                    for(auto& arg: cmd.args) {
                        std::println("  Arg: {}", arg);
                    }

                    rpc::data::decision_info decision{
                        .act =
                            {
                                  .type = rpc::data::action::WRAP,
                                  .cmd = cmd,
                                  },
                        .nxt_cmd_id = parent_id + 1,
                    };

                    auto ret = co_await uv::async::write(
                        uv::cast<uv_stream_t>(client),
                        Serde<rpc::data::decision_info>::serialize(decision));
                    if(ret < 0) {
                        throw std::runtime_error(uv_strerror(ret));
                    }
                    break;
                }
                case rpc::data::Request::FINISH: {
                    std::println("Received FINISH request.");
                    int ret_code = co_await Serde<int>::co_deserialize(reader);
                    std::println("Finish code: {}", ret_code);
                    break;
                }
                case rpc::data::Request::REPORT_ERROR: {
                    std::println("Received REPORT_ERROR request.");
                    rpc::data::command_id_t parent_id =
                        co_await Serde<rpc::data::command_id_t>::co_deserialize(reader);
                    std::println("Parent ID: {}", parent_id);
                    std::string error_msg = co_await Serde<std::string>::co_deserialize(reader);
                    std::println("Error message: {}", error_msg);
                    break;
                }
                default: std::println("Unknown request received.");
            }
        }
    } catch(std::exception& e) {
        std::println("Exception while handling request: {}", e.what());
    }
    co_return;
}

uv::async::Lazy<void> loop() {
    auto server = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());
    if(auto ret = uv_pipe_bind(server, catter::config::rpc::PIPE_NAME); ret < 0) {
        std::println("Bind error: {}", uv_strerror(ret));
        co_return;
    }

    std::vector<uv::async::Lazy<void>> acceptors;
    server->data = &acceptors;
    auto ret = uv_listen(uv::cast<uv_stream_t>(server), 128, [](uv_stream_t* server, int status) {
        if(status < 0) {
            std::println("Listen error: {}", uv_strerror(status));
            return;
        }
        auto& acceptors = *static_cast<std::vector<uv::async::Lazy<void>>*>(server->data);
        acceptors.push_back(accept(server));
    });

    if(ret < 0) {
        std::println("Listen error: {}", uv_strerror(ret));
        co_return;
    }

    // std::string exe_path = "catter-proxy.exe";

    // std::vector<std::string> args = {"-p", "42", "--", "ls"};

    // auto proxy_ret = co_await uv::async::spawn(exe_path, args);

    // std::println("catter-proxy exited with code {}", proxy_ret);

    co_await std::suspend_always{};

    for(auto& acceptor: acceptors) {
        if(!acceptor.done()) {
            std::println("Error: acceptor coroutine not done yet.");
        }
    }
    co_return;
}

uv::async::Lazy<int64_t> spawn() {
    std::string path = "ls";

    std::vector<std::string> args = {};

    std::vector<const char*> line;
    line.emplace_back(path.c_str());
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

    options.file = path.c_str();
    options.args = const_cast<char**>(line.data());
    options.flags = UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS | UV_PROCESS_WINDOWS_HIDE;
    options.stdio_count = 3;
    options.stdio = child_stdio;
    options.exit_cb = [](uv_process_t* process, int64_t exit_status, int term_signal) {
        std::println("Process exited with status {} and signal {}", exit_status, term_signal);
    };

    // auto process = co_await uv::async::Create<uv_process_t>();
    uv_process_t process{};

    struct awaiter {
        bool await_ready() {
            auto ret = uv_spawn(uv::default_loop(), process, options);
            if(ret < 0) {
                throw std::runtime_error(uv_strerror(ret));
            }
            return false;
        }

        int64_t await_resume() noexcept {
            return 42;
        }

        void await_suspend(std::coroutine_handle<>) noexcept {}

        uv_process_t* process;
        uv_process_options_t* options;
    };

    // co_return co_await awaiter{&process, &options};

    co_return co_await uv::async::awaiter::Spawn{uv::default_loop(), &options};
}

int main(int argc, char* argv[], char* envp[]) {
    try {
        auto task = spawn();
        task.get();
    } catch(const std::exception& e) {
        std::println("Exception in main: {}", e.what());
        return 0;
    }
    return 0;
}
