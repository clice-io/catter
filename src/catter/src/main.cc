#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <uv.h>
#include <vector>
#include <string>
#include <print>
#include <ranges>
#include <algorithm>

#include "libconfig/rpc.h"
#include "libutil/lazy.h"
#include "libutil/uv.h"
#include "libutil/rpc_data.h"

using namespace catter;

static int id_generator = 0;

uv::async::Lazy<void> accept(uv_stream_t* server) {
    auto id = ++id_generator;

    auto client = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());
    uv_accept(server, uv::cast<uv_stream_t>(client));

    auto reader = [&](char* dst, size_t len) -> coro::Lazy<void> {
        auto ret = co_await uv::async::read(uv::cast<uv_stream_t>(client), dst, len);
        if(ret < 0) {
            throw ret;
        }
    };

    try {
        while(true) {
            rpc::data::Request req = co_await Serde<rpc::data::Request>::co_deserialize(reader);
            switch(req) {
                case rpc::data::Request::MAKE_DECISION: {
                    rpc::data::command_id_t parent_id =
                        co_await Serde<rpc::data::command_id_t>::co_deserialize(reader);
                    rpc::data::command cmd =
                        co_await Serde<rpc::data::command>::co_deserialize(reader);

                    std::string line = cmd.executable;
                    for(auto& arg: cmd.args) {
                        line.append(std::format(" {}", arg));
                    }
                    std::println("ID [{}] created from [{}]: {}", id, parent_id, line);

                    rpc::data::decision_info decision{
                        {
                         rpc::data::action::INJECT,
                         cmd, },
                        id
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
                    int ret_code = co_await Serde<int>::co_deserialize(reader);
                    std::println("ID [{}] finish code: {}", id, ret_code);
                    break;
                }
                case rpc::data::Request::REPORT_ERROR: {
                    rpc::data::command_id_t parent_id =
                        co_await Serde<rpc::data::command_id_t>::co_deserialize(reader);
                    std::string error_msg = co_await Serde<std::string>::co_deserialize(reader);
                    std::println("ID [{}] reported error: {}", parent_id, error_msg);
                    break;
                }
                default: std::println("Unknown request received.");
            }
        }
    } catch(ssize_t err) {
        if(err == UV_EOF) {
            std::println("ID [{}] disconnected.", id);
        } else {
            std::println("ID [{}] disconnected with error: {}", id, uv_strerror(err));
        }
    } catch(const std::exception& ex) {
        std::println("Exception while handling request: {}", ex.what());
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

    std::string exe_path = "/home/seele/catter/build/linux/x86_64/debug/catter-proxy";

    std::vector<std::string> args = {"-p", std::to_string(++id_generator), "--", "make", "-j"};

    auto proxy_ret = co_await uv::async::spawn(exe_path, args, true);

    std::println("catter-proxy exited with code {}", proxy_ret);

    for(auto& acceptor: acceptors) {
        if(!acceptor.done()) {
            std::println("Error: acceptor coroutine not done yet.");
        } else {
            try {
                acceptor.get();
            } catch(const std::exception& ex) {
                std::println("Exception in acceptor coroutine: {}", ex.what());
            }
        }
    }
    co_return;
}

int main() {
    try {
        uv::wait(loop());
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}
