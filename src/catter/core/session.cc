#include <cassert>
#include <format>
#include <list>
#include <ranges>
#include <stdexcept>
#include <string>

#include <eventide/async/io/loop.h>

#include "session.h"

#include "util/guard.h"
#include "util/log.h"
#include "util/crossplat.h"
#include "util/eventide.h"
#include "config/ipc.h"
#include "config/catter-proxy.h"

namespace catter {

int64_t Session::run(RunPlan run_plan) {
#ifndef _WIN32
    if(std::filesystem::exists(config::ipc::pipe_name())) {
        std::filesystem::remove(config::ipc::pipe_name());
    }
#endif
    auto acc_ret =
        eventide::pipe::listen(config::ipc::pipe_name(), eventide::pipe::options(), default_loop());

    if(!acc_ret) {
        throw std::runtime_error(
            std::format("Failed to create acceptor: {}", acc_ret.error().message()));
    }

    this->acc = std::make_unique<PipeAcceptor>(std::move(*acc_ret));

    auto loop_task = this->loop(std::move(run_plan.callback));
    auto spawn_task = this->spawn(std::move(run_plan.launch_plan.executable),
                                  std::move(run_plan.launch_plan.args),
                                  std::move(run_plan.launch_plan.cwd));

    default_loop().schedule(loop_task);
    default_loop().schedule(spawn_task);

    default_loop().run();

    loop_task.result();  // Propagate exceptions from loop task
    return spawn_task.result();
}

eventide::task<void> Session::loop(ClientAcceptor acceptor) {
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
        eventide::event_loop::current().schedule(linked_clients.back());
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

eventide::task<int64_t> Session::spawn(std::string executable,
                                       std::vector<std::string> args,
                                       std::string cwd) {
    // for exception safety: ensure acceptor is stopped when spawn exits, since spawn failure should
    // prevent the session from running
    auto guard = util::make_guard([&]() noexcept {
        if(this->acc) {
            auto err = this->acc->stop();
            this->acc.reset();
            if(err.has_error()) {
                LOG_ERROR("Failed to stop acceptor: {}", err.message());
            }
        }
    });

    eventide::process::options opts{
        .file = executable,
        .args = args,
        .cwd = cwd,
        .creation = {.windows_hide = true, .windows_verbatim_arguments = true},
        .streams = {eventide::process::stdio::ignore(),
                     eventide::process::stdio::ignore(),
                     eventide::process::stdio::ignore()}
    };

    std::string args_str;
    for(const auto& arg: args) {
        args_str += std::format("{} ", arg);
    }

    LOG_INFO("Spawning process: \n    exe = {} \n    cwd = {} \n    args = {}",
             executable,
             cwd,
             args_str);

    auto spawn_ret = eventide::process::spawn(opts, eventide::event_loop::current());
    if(!spawn_ret) {
        throw std::runtime_error(
            std::format("process spawn failed: {}", spawn_ret.error().message()));
    }

    auto exit_status = co_await spawn_ret->proc.wait();
    if(exit_status.has_error()) {
        throw std::runtime_error(
            std::format("process wait failed: {}", exit_status.error().message()));
    }

    auto [ret, signal] = *exit_status;
    co_return ret;
}

}  // namespace catter
