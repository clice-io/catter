module;

#include "modules/prelude.h"

#include "support/guard.h"
#include "support/log.h"
#include "support/messages.h"

module catter;

import :session.handler;
import :session.process;
import :session.session;

namespace catter {

data::ipcid_t Session::new_id() noexcept {
    last_id += 1;
    return last_id;
}

kota::task<data::action> Session::decide(data::ipcid_t id,
                                         std::optional<data::ipcid_t> parent,
                                         data::command command) {
    if(error) {
        co_return data::action{.type = data::action::FAIL, .cmd = std::move(command)};
    }
    Action act;
    try {
        act = co_await handler.on_command(id, parent, command);
    } catch(...) {
        fail(std::current_exception());
        co_return data::action{.type = data::action::FAIL, .cmd = std::move(command)};
    }

    switch(act.type) {
        case Action::Type::Drop: {
            co_return data::action{.type = data::action::DROP, .cmd = std::move(command)};
        }
        case Action::Type::Skip: {
            co_return data::action{.type = data::action::RUN, .cmd = std::move(command)};
        }
        case Action::Type::Modify: {
            co_return data::action{.type = data::action::RUN, .cmd = std::move(act.command)};
        }
    }
    std::unreachable();
}

kota::task<> Session::exited(data::ipcid_t id, data::process_result result) {
    if(error) {
        co_return;
    }
    try {
        co_await handler.on_exit(id, std::move(result));
    } catch(...) {
        fail(std::current_exception());
    }
}

kota::task<> Session::uncaptured(data::ipcid_t id,
                                 std::optional<data::ipcid_t> parent,
                                 std::string message) {
    if(error) {
        co_return;
    }
    try {
        co_await handler.on_error(id, parent, std::move(message));
    } catch(...) {
        fail(std::current_exception());
    }
}

void Session::fail(std::exception_ptr error) noexcept {
    if(!this->error) {
        this->error = std::move(error);
    }
}

void Session::rethrow() const {
    if(error) {
        std::rethrow_exception(error);
    }
}

kota::task<kota::process::exit_status> Session::serve(std::string pipe_name,
                                                      Start start,
                                                      Accept accept) {
#ifndef CATTER_WINDOWS
    // A socket a session of the same pid left behind.
    kota::fs::remove(pipe_name);
#endif
    auto& current_loop = kota::event_loop::current();
    auto acc_ret = kota::pipe::listen(pipe_name, kota::pipe::options(), current_loop);

    if(!acc_ret) {
        throw cpptrace::runtime_error(
            std::format("Failed to listen on `{}` for the build's commands: {}",
                        pipe_name,
                        acc_ret.error().message()));
    }

    this->acc = std::make_unique<kota::acceptor<kota::pipe>>(std::move(*acc_ret));

    auto loop_task = this->loop(std::move(accept));
    auto start_task = this->start(std::move(start));

    auto [_, status] = co_await kota::when_all{std::move(loop_task), std::move(start_task)};
#ifndef CATTER_WINDOWS
    kota::fs::remove(pipe_name);
#endif
    co_return status;
}

kota::task<void> Session::loop(Accept accept) {
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
        linked_clients.push_back(accept(id, std::move(*client)));
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

kota::task<kota::process::exit_status> run_session(Handler& handler,
                                                   const Backend& backend,
                                                   Build build) {
    auto started = co_await handler.on_start(std::move(build));
    if(!started) {
        co_return kota::process::exit_status{};
    }
    Session session(handler);
    const auto start = std::chrono::steady_clock::now();
    auto status = co_await backend.run(*started, session);
    session.rethrow();
    co_await handler.on_finish(result_of(status, start));
    co_return status;
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
