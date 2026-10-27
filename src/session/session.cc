module;

#include "modules/prelude.h"

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

}  // namespace catter
