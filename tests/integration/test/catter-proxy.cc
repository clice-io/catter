

// clang-format off
// RUN: %it_catter_proxy
// clang-format on
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <list>
#include <memory>
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

#include "config/ipc.h"
#include "config/catter-proxy.h"
#include "util/crossplat.h"
#include "util/eventide.h"
#include "util/serde.h"
#include "util/ipc-data.h"
#include "opt-data/catter/table.h"

using namespace catter;

using acceptor = eventide::acceptor<eventide::pipe>;

eventide::task<void> spawn(std::shared_ptr<acceptor> acceptor) {
    // co_await std::suspend_always{};  // placeholder

    std::string exe_path =
        (util::get_catter_root_path() / catter::config::proxy::EXE_NAME).string();

    std::vector<std::string> args = {exe_path, "-p", "0", "--exec", "echo", "--", "Hello, World!"};

    eventide::process::options opts{
        .file = exe_path,
        .args = args,
        .creation = {.windows_hide = true, .windows_verbatim_arguments = true},
        .streams = {eventide::process::stdio::ignore(),
                     eventide::process::stdio::ignore(),
                     eventide::process::stdio::ignore()}
    };
    auto ret = co_await catter::spawn(opts);
    auto error = acceptor->stop();  // Stop accepting new clients after spawning the process
    if(error) {
        std::println("Failed to stop acceptor: {}", error.message());
    }
    co_return;
}

eventide::task<void> accept(eventide::pipe client) {

    auto reader = [&](char* dst, size_t len) -> eventide::task<void> {
        size_t total_read = 0;
        while(total_read < len) {
            auto ret = co_await client.read_some({dst + total_read, len - total_read});
            if(ret == 0) {
                throw total_read;  // EOF
            }
            total_read += ret;
        }
        co_return;
    };

    bool create_received = false;
    bool finish_received = false;
    bool decision_received = false;

    try {
        while(true) {
            ipc::data::Request req = co_await Serde<ipc::data::Request>::co_deserialize(reader);
            switch(req) {
                case ipc::data::Request::CREATE: {
                    create_received = true;

                    ipc::data::command_id_t parent_id =
                        co_await Serde<ipc::data::command_id_t>::co_deserialize(reader);
                    std::println("create with parent ID [{}]", parent_id);
                    auto err = co_await client.write(Serde<ipc::data::command_id_t>::serialize(0));

                    if(err.has_error()) {
                        throw std::runtime_error(
                            std::format("Failed to send command ID to client: {}", err.message()));
                    }

                    break;
                }

                case ipc::data::Request::MAKE_DECISION: {
                    decision_received = true;
                    ipc::data::command cmd =
                        co_await Serde<ipc::data::command>::co_deserialize(reader);

                    std::string line = std::format("exe = {} args =", cmd.executable);

                    for(auto& arg: cmd.args) {
                        line.append(std::format(" {}", arg));
                    }

                    auto act = ipc::data::action{
                        .type = ipc::data::action::WRAP,
                        .cmd = cmd,
                    };

                    std::println("decision: {}", line);

                    auto err = co_await client.write(Serde<ipc::data::action>::serialize(act));
                    if(err.has_error()) {
                        throw std::runtime_error(
                            std::format("Failed to send action to client: {}", err.message()));
                    }

                    break;
                }
                case ipc::data::Request::FINISH: {
                    finish_received = true;
                    int ret_code = co_await Serde<int>::co_deserialize(reader);
                    std::println("finish code: {}", ret_code);
                    break;
                }
                case ipc::data::Request::REPORT_ERROR: {
                    auto parent_id =
                        co_await Serde<ipc::data::command_id_t>::co_deserialize(reader);
                    auto cmd_id = co_await Serde<ipc::data::command_id_t>::co_deserialize(reader);
                    std::string error_msg = co_await Serde<std::string>::co_deserialize(reader);
                    throw std::runtime_error(
                        std::format("Error reported from parent ID [{}], command ID [{}]: {}",
                                    parent_id,
                                    cmd_id,
                                    error_msg));
                    break;
                }
                default: std::println("Unknown request received.");
            }
        }
    } catch(size_t err) {
        if(err == 0) {
            std::println("Client disconnected.");
        } else {
            throw std::runtime_error("Error while reading from client: unexpected EOF");
        }
    }

    std::string missing_reqs;
    if(!create_received)
        missing_reqs.append("CREATE ");
    if(!decision_received)
        missing_reqs.append("MAKE_DECISION ");
    if(!finish_received)
        missing_reqs.append("FINISH ");
    if(!missing_reqs.empty()) {
        throw std::runtime_error(
            std::format("Client disconnected before sending all expected requests. Missing: {}",
                        missing_reqs));
    }
    co_return;
}

eventide::task<void> loop(std::shared_ptr<acceptor> acceptor) {
    std::list<eventide::task<void>> linked_clients;
    while(true) {
        auto client = co_await acceptor->accept();
        if(!client) {
            if(client.error() != eventide::error::operation_aborted) {
                // Accept can fail with operation_aborted when the acceptor is stopped, which is
                // expected
                std::println("Failed to accept client: {}", client.error().message());
            }
            break;
        }
        linked_clients.push_back(accept(std::move(*client)));
        default_loop().schedule(linked_clients.back());
    }

    for(auto& client_task: linked_clients) {
        client_task.result();  // Await completion and propagate exceptions
    }
    co_return;
}

int main(int argc, char* argv[]) {
#ifndef _WIN32
    if(std::filesystem::exists(catter::config::ipc::PIPE_NAME)) {
        std::filesystem::remove(catter::config::ipc::PIPE_NAME);
    }
#endif

    auto acc_ret = eventide::pipe::listen(catter::config::ipc::PIPE_NAME,
                                          eventide::pipe::options(),
                                          default_loop());

    if(!acc_ret) {
        std::println("Failed to create pipe server: {}", acc_ret.error().message());
        return 1;
    }

    try {
        auto acc = std::make_shared<acceptor>(std::move(*acc_ret));
        // becareful, msvc has a bug that asan will report false positive UAF
        default_loop().schedule(loop(acc));
        default_loop().schedule(spawn(acc));
        acc.reset();  // We can release our reference to the acceptor since the loop task will keep
                      // it alive
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
