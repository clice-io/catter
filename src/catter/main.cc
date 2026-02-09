#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <list>
#include <stdexcept>
#include <vector>
#include <string>
#include <print>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <format>
#include <print>

#include <eventide/process.h>
#include <eventide/stream.h>
#include <eventide/loop.h>
#include <reflection/name.h>

#include "js.h"
#include "config/ipc.h"
#include "config/catter-proxy.h"
#include "util/crossplat.h"
#include "util/eventide.h"
#include "util/serde.h"
#include "util/ipc-data.h"

using namespace catter;

static int id_generator = 0;

void print_hex(char* data, size_t len) {
    for(size_t i = 0; i < len; ++i) {
        std::print("{:02x} ", static_cast<unsigned char>(data[i]));
    }
    std::print(" -> ");
    for(size_t i = 0; i < len; ++i) {
        char c = data[i];
        std::print("{}", (std::isprint(static_cast<unsigned char>(c)) ? c : '.'));
    }
    std::println();
}

eventide::task<void> spawn(std::vector<std::string> shell,
                           eventide::acceptor<eventide::pipe>& acceptor) {
    co_await std::suspend_always{};  // placeholder

    std::string exe_path =
        (util::get_catter_root_path() / catter::config::proxy::EXE_NAME).string();

    std::vector<std::string> args = {"-p", std::to_string(id_generator), "--"};

    append_range_to_vector(args, shell);

    eventide::process::options opts{
        .file = exe_path,
        .args = args,
        .creation =
            {
                       .windows_hide = true,
                       .windows_verbatim_arguments = true,
                       },
        .streams = {
                       eventide::process::stdio::ignore(),
                       eventide::process::stdio::ignore(),
                       eventide::process::stdio::ignore(),
                       }
    };
    auto ret = co_await catter::spawn(opts);
    // acceptor.stop();  // Stop accepting new clients after spawning the process
    co_return;
}

eventide::task<void> accept(eventide::pipe client) {
    auto id = ++id_generator;

    auto reader = [&](char* dst, size_t len) -> eventide::task<void> {
        size_t total_read = 0;
        while (total_read < len) {
            auto ret = co_await client.read_some({dst + total_read, len - total_read});
            if (ret == 0) {
                throw total_read;  // EOF
            }
            total_read += ret;
        }
        co_return;
    };

    try {
        while(true) {
            ipc::data::Request req = co_await Serde<ipc::data::Request>::co_deserialize(reader);
            switch(req) {
                case ipc::data::Request::CREATE: {
                    ipc::data::command_id_t parent_id =
                        co_await Serde<ipc::data::command_id_t>::co_deserialize(reader);

                    std::println("ID [{}] created from [{}]", id, parent_id);

                    // TODO
                    co_await client.write(Serde<ipc::data::command_id_t>::serialize(id));
                    break;
                }

                case ipc::data::Request::MAKE_DECISION: {
                    ipc::data::command cmd =
                        co_await Serde<ipc::data::command>::co_deserialize(reader);

                    std::string line = std::format("exe = {} args =", cmd.executable);

                    for(auto& arg: cmd.args) {
                        line.append(std::format(" {}", arg));
                    }

                    auto act = ipc::data::action{
                        .type = ipc::data::action::INJECT,
                        .cmd = cmd,
                    };

                    std::println("ID [{}] decision: {}", id, line);

                    // TODO
                    co_await client.write(Serde<ipc::data::action>::serialize(act));

                    break;
                }
                case ipc::data::Request::FINISH: {
                    int ret_code = co_await Serde<int>::co_deserialize(reader);
                    std::println("ID [{}] finish code: {}", id, ret_code);
                    break;
                }
                case ipc::data::Request::REPORT_ERROR: {
                    auto parent_id =
                        co_await Serde<ipc::data::command_id_t>::co_deserialize(reader);
                    auto cmd_id = co_await Serde<ipc::data::command_id_t>::co_deserialize(reader);
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
    } catch(size_t err) {
        if(err == 0) {
            std::println("ID [{}] disconnected.", id);
        }
    } catch(const std::exception& ex) {
        std::println("Exception while handling request: {}", ex.what());
    }
    co_return;
}

eventide::task<void> loop(eventide::acceptor<eventide::pipe>& acceptor) {
    std::vector<eventide::task<void>> linked_clients;
    while(true) {
        auto client = co_await acceptor.accept();
        if(!client) {
            std::println("Failed to accept client: {}", client.error().message());
            break;
        }
        std::println("Client connected.");
        linked_clients.push_back(accept(std::move(*client)));
        default_loop().schedule(linked_clients.back());
    }

    try {
        for(auto& client_task: linked_clients) {
            client_task.result();  // Await completion and propagate exceptions
        }
    } catch(const std::exception& ex) {}
}

int main(int argc, char* argv[]) {
#ifndef _WIN32
    if(std::filesystem::exists(catter::config::ipc::PIPE_NAME)) {
        std::filesystem::remove(catter::config::ipc::PIPE_NAME);
    }
#endif

    if(argc < 2 || std::string(argv[1]) != "--") {
        std::println("Usage: catter -- <target program> [args...]");
        return 1;
    }

    std::vector<std::string> shell;

    for(int i = 2; i < argc; ++i) {
        shell.push_back(argv[i]);
    }

    auto acceptor = eventide::pipe::listen(catter::config::ipc::PIPE_NAME,
                                           eventide::pipe::options(),
                                           default_loop());

    if(!acceptor) {
        std::println("Failed to create pipe server: {}", acceptor.error().message());
        return 1;
    }

    try {
        auto loop_task = loop(*acceptor);
        auto spawn_task = spawn(shell, *acceptor);
        default_loop().schedule(loop_task);
        default_loop().schedule(spawn_task);
        default_loop().run();
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}
