#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <uv.h>
#include <vector>
#include <string>
#include <print>

#include "libconfig/rpc.h"
#include "libutil/uv.h"
#include "libutil/rpc_data.h"

using namespace catter;


uv::async::Lazy<void> accept(uv_stream_t* server) {
    auto client = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());
    uv_accept(server, uv::cast<uv_stream_t>(client));

    auto reader = [&](char* dst, size_t len) -> coro::Lazy<void> {
        auto ret = co_await uv::async::read(uv::cast<uv_stream_t>(client), dst, len);
        std::print("Read {} bytes: ", ret);
        for(size_t i = 0; i < static_cast<size_t>(ret); ++i) {
            std::print("{:02x} ", static_cast<uint8_t>(dst[i]));
        }
        std::println();
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
                        .act = {
                            .type = rpc::data::action::WRAP,
                            .cmd = cmd,
                        },
                        .nxt_cmd_id = parent_id + 1,
                    };

                    auto ret = co_await uv::async::write(
                        uv::cast<uv_stream_t>(client),
                        Serde<rpc::data::decision_info>::serialize(decision));
                    std::println("Sent decision info ({} bytes).", ret);
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
                    std::string error_msg =
                        co_await Serde<std::string>::co_deserialize(reader);
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

    std::string exe_path = "catter-proxy";

    std::vector<std::string> args = {"-p", "42", "--", "ls"};

    auto proxy_ret = co_await uv::async::spawn(exe_path, args);

    std::println("catter-proxy exited with code {}", proxy_ret);

    for(auto& acceptor: acceptors) {
        if (!acceptor.done()) {
            std::println("Error: acceptor coroutine not done yet.");
        }
    }
    co_return;
}


int main(int argc, char* argv[], char* envp[]) {
    auto loop_task = loop();

    uv::run();
    try {
        loop_task.get();
    } catch(std::exception& e) {
        std::println("Exception: {}", e.what());
    }
    return 0;
}
