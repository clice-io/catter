#include <cassert>
#include <filesystem>
#include <format>
#include <functional>
#include <list>
#include <ranges>
#include <stdexcept>
#include <string>

#include "session.h"

#include "util/log.h"
#include "util/crossplat.h"
#include "util/eventide.h"
#include "config/ipc.h"
#include "config/catter-proxy.h"

namespace catter {

eventide::task<void> Session::loop(
    util::function_ref<eventide::task<void>(data::ipcid_t, eventide::pipe&&)> acceptor) {
    std::list<eventide::task<void>> linked_clients;
    for(auto i: std::views::iota(data::ipcid_t(1))) {
        auto client = co_await this->acc->accept();
        if(!client) {
            assert(client.error() == eventide::error::operation_aborted);
            // Accept can fail with operation_aborted when the acceptor is stopped, which is
            // expected
            break;
        }
        linked_clients.push_back(acceptor(i, std::move(*client)));
        default_loop().schedule(linked_clients.back());
        LOG_INFO("Accepted new client with id: {}", i);
    }

    std::string error_msg;

    for(auto& client_task: linked_clients) {
        try {
            client_task.result();  // Await completion and propagate exceptions
        } catch(const std::exception& ex) {
            error_msg += std::format("Exception in client task: {}\n", ex.what());
        }
    }
    if(!error_msg.empty()) {
        throw std::runtime_error(error_msg);
    }
    co_return;
}

eventide::task<int64_t> Session::spawn(std::string executable, std::vector<std::string> args) {
    eventide::process::options opts{
        .file = executable,
        .args = args,
        .creation = {.windows_hide = true, .windows_verbatim_arguments = true},
        .streams = {eventide::process::stdio::ignore(),
                     eventide::process::stdio::ignore(),
                     eventide::process::stdio::ignore()}
    };

    std::string args_str;
    for(const auto& arg: args) {
        args_str += std::format("{} ", arg);
    }

    LOG_INFO("Spawning process: \n    exe = {} \n    args = {}", executable, args_str);

    auto ret = co_await catter::spawn(opts);

    auto error = this->acc->stop();  // Stop accepting new clients after spawning the process
    if(error) {
        throw std::runtime_error(std::format("Failed to stop acceptor: {}", error.message()));
    }

    this->acc.reset();  // Ensure acceptor is destroyed after stopping
    co_return ret;
}

}  // namespace catter
