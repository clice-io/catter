module;

#include "modules/prelude.h"

#include "support/guard.h"
#include "support/log.h"
#include "support/messages.h"

module catter;

import :session.session;

namespace catter {

kota::task<kota::process::exit_status> Session::run(RunPlan run_plan) {
#ifndef CATTER_WINDOWS
    // A socket a session of the same pid left behind.
    kota::fs::remove(run_plan.pipe_name);
#endif
    auto& current_loop = kota::event_loop::current();
    auto acc_ret = kota::pipe::listen(run_plan.pipe_name, kota::pipe::options(), current_loop);

    if(!acc_ret) {
        throw cpptrace::runtime_error(
            std::format("Failed to listen on `{}` for the build's commands: {}",
                        run_plan.pipe_name,
                        acc_ret.error().message()));
    }

    this->acc = std::make_unique<PipeAcceptor>(std::move(*acc_ret));

    auto loop_task = this->loop(std::move(run_plan.callback));
    auto start_task = this->start(std::move(run_plan.start));

    auto [_, status] = co_await kota::when_all{std::move(loop_task), std::move(start_task)};
#ifndef CATTER_WINDOWS
    kota::fs::remove(run_plan.pipe_name);
#endif
    co_return status;
}

data::ipcid_t Session::new_id() noexcept {
    last_id += 1;
    return last_id;
}

kota::task<void> Session::loop(ClientAcceptor acceptor) {
    std::list<kota::task<void>> linked_clients;
    while(true) {
        auto client = co_await this->acc->accept();
        if(!client) {
            assert(client.error() == kota::error::operation_aborted);
            // Accept can fail with operation_aborted when the acceptor is stopped, which is
            // expected
            break;
        }
        const auto id = new_id();
        linked_clients.push_back(acceptor(id, std::move(*client)));
        kota::event_loop::current().schedule(linked_clients.back());
        LOG_INFO("Accepted new client with id: {}", id);
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

kota::task<kota::process::exit_status> Session::start(Start start) {
    // for exception safety: ensure acceptor is stopped when the build ends, since a build that
    // fails to start should prevent the session from running
    auto guard = make_guard([&]() noexcept {
        if(this->acc) {
            auto err = this->acc->stop();
            this->acc.reset();
            if(err.has_error()) {
                LOG_ERROR("Failed to stop acceptor: {}", err.message());
            }
        }
    });
    co_return co_await start();
}

}  // namespace catter
