module;

#include "modules/prelude.h"

#include "support/config/catter-proxy.h"
#include "support/config/ipc.h"
#include "support/guard.h"
#include "support/log.h"

module catter;

import :session.process;
import :session.session;

namespace catter {

kota::task<data::process_result> Session::run(RunPlan run_plan) {
#ifndef CATTER_WINDOWS
    // A socket a session of the same pid left behind.
    kota::fs::remove(run_plan.pipe_name);
#endif
    auto& current_loop = kota::event_loop::current();
    auto acc_ret = kota::pipe::listen(run_plan.pipe_name, kota::pipe::options(), current_loop);

    if(!acc_ret) {
        throw cpptrace::runtime_error(
            std::format("Failed to create acceptor `{}`", acc_ret.error().message()));
    }

    this->acc = std::make_unique<PipeAcceptor>(std::move(*acc_ret));

    auto loop_task = this->loop(std::move(run_plan.callback));
    auto spawn_task = this->spawn(std::move(run_plan.launch_plan.executable),
                                  std::move(run_plan.launch_plan.args),
                                  std::move(run_plan.launch_plan.cwd));

    auto [_, process_result] = co_await kota::when_all{std::move(loop_task), std::move(spawn_task)};
#ifndef CATTER_WINDOWS
    kota::fs::remove(run_plan.pipe_name);
#endif
    co_return std::move(process_result);
}

kota::task<void> Session::loop(ClientAcceptor acceptor) {
    std::list<kota::task<void>> linked_clients;
    for(auto i: std::views::iota(data::ipcid_t(1))) {
        auto client = co_await this->acc->accept();
        if(!client) {
            assert(client.error() == kota::error::operation_aborted);
            // Accept can fail with operation_aborted when the acceptor is stopped, which is
            // expected
            break;
        }
        linked_clients.push_back(acceptor(i, std::move(*client)));
        kota::event_loop::current().schedule(linked_clients.back());
        LOG_INFO("Accepted new client with id: {}", i);
    }

    std::string error_msg;

    for(auto& client_task: linked_clients) {
        // A client still connected is let go, cancelled, when the list goes; result() requires a
        // task that has ended.
        if(!client_task.done()) {
            continue;
        }
        try {
            client_task.result();  // Propagate exceptions
        } catch(const std::exception& ex) {
            error_msg += std::format("|| {} ||", ex.what());
        }
    }
    if(!error_msg.empty()) {
        throw cpptrace::runtime_error(std::move(error_msg));
    }
    co_return;
}

kota::task<data::process_result> Session::spawn(std::string executable,
                                                std::vector<std::string> args,
                                                std::string cwd) {
    // for exception safety: ensure acceptor is stopped when spawn exits, since spawn failure should
    // prevent the session from running
    auto guard = make_guard([&]() noexcept {
        if(this->acc) {
            auto err = this->acc->stop();
            this->acc.reset();
            if(err.has_error()) {
                LOG_ERROR("Failed to stop acceptor: {}", err.message());
            }
        }
    });

    std::string args_str;
    for(const auto& arg: args) {
        args_str += std::format("{} ", arg);
    }

    LOG_INFO("Spawning process: \n    exe = {} \n    cwd = {} \n    args = {}",
             executable,
             cwd,
             args_str);

    // The build gets the signals as catter was given them, as nohup leaves
    // them for it.
    kota::process::options opts{
        .program = std::move(executable),
        .args = std::move(args),
        .cwd = std::move(cwd),
        .creation = {.keep_signals = true},
    };
    auto status = co_await run_process(std::move(opts));
    co_return result_of(status);
}

}  // namespace catter
