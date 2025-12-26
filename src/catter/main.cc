

#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <vector>
#include <string>
#include <print>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <format>
#include <print>

#include <uv.h>

#include "js.h"

#include "config/rpc.h"
#include "config/catter-proxy.h"

#include "util/crossplat.h"
#include "util/lazy.h"
#include "uv/uv.h"
#include "uv/rpc_data.h"

using namespace catter;

static int id_generator = 0;

uv::async::Lazy<void> accept(uv_stream_t* server) {
    auto id = ++id_generator;

    auto client = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());
    if(auto ret = uv_accept(server, uv::cast<uv_stream_t>(client)); ret < 0) {
        std::println("Accept error: {}", uv_strerror(ret));
        co_return;
    }

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
                case rpc::data::Request::CREATE: {
                    rpc::data::command_id_t parent_id =
                        co_await Serde<rpc::data::command_id_t>::co_deserialize(reader);

                    std::println("ID [{}] created from [{}]", id, parent_id);

                    auto ret =
                        co_await uv::async::write(uv::cast<uv_stream_t>(client),
                                                  Serde<rpc::data::command_id_t>::serialize(id));
                    if(ret < 0) {
                        throw std::runtime_error(uv_strerror(ret));
                    }
                    break;
                }

                case rpc::data::Request::MAKE_DECISION: {
                    rpc::data::command cmd =
                        co_await Serde<rpc::data::command>::co_deserialize(reader);

                    std::string line = cmd.executable;

                    for(auto& arg: cmd.args) {
                        line.append(std::format(" {}", arg));
                    }

                    auto act = rpc::data::action{
                        .type = rpc::data::action::WRAP,
                        .cmd = cmd,
                    };

                    std::println("ID [{}] decision: {}", id, line);

                    auto ret = co_await uv::async::write(uv::cast<uv_stream_t>(client),
                                                         Serde<rpc::data::action>::serialize(act));

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
                    auto parent_id =
                        co_await Serde<rpc::data::command_id_t>::co_deserialize(reader);
                    auto cmd_id = co_await Serde<rpc::data::command_id_t>::co_deserialize(reader);
                    std::string error_msg = co_await Serde<std::string>::co_deserialize(reader);
                    std::println("ID [{}] from [{}] reported error: {}",
                                 cmd_id,
                                 parent_id,
                                 error_msg);

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

    auto listen_cb = [&](uv_stream_t* server, int status) {
        if(status < 0) {
            std::println("Listen error: {}", uv_strerror(status));
            return;
        }
        acceptors.push_back(accept(server));
    };

    auto ret = uv::listen(uv::cast<uv_stream_t>(server), 128, listen_cb);

    if(ret < 0) {
        std::println("Listen error: {}", uv_strerror(ret));
        co_return;
    }

    co_await std::suspend_always{};  // placeholder to keep the server running

    auto exe_path = util::get_catter_root_path() / catter::config::proxy::EXE_NAME;

    std::vector<std::string> args = {"-p", std::to_string(++id_generator), "--", "make", "-j"};

    auto proxy_ret = co_await uv::async::spawn(exe_path.string(), args, true);

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

int main(int argc, char* argv[], char* envp[]) {
    catter::core::js::init_qjs({.pwd = std::filesystem::current_path()});
    try {
        catter::core::js::run_js_file(
            R"(
        import * as catter from "catter";
        catter.o.print("Hello from Catter!");
    )",
            "inline.js");

        uv::wait(loop());
    } catch(const catter::qjs::Exception& ex) {
        std::println("JavaScript Exception: {}", ex.what());
        return 1;
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}
