module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter;

import :session.handler;

namespace catter {

HandlerChain::HandlerChain(std::vector<Handler*> handlers) : handlers(std::move(handlers)) {}

kota::task<std::optional<Build>> HandlerChain::on_start(Build build) {
    for(auto* handler: handlers) {
        auto next = co_await handler->on_start(std::move(build));
        if(!next) {
            co_return std::nullopt;
        }
        build = std::move(*next);
    }
    co_return build;
}

kota::task<Action> HandlerChain::on_command(data::ipcid_t id,
                                            std::optional<data::ipcid_t> parent,
                                            data::command command) {
    Action result{.type = Action::Type::Skip};
    for(auto* handler: handlers) {
        auto action = co_await handler->on_command(id, parent, command);
        switch(action.type) {
            case Action::Type::Skip: break;
            case Action::Type::Drop: co_return action;
            case Action::Type::Modify: {
                command = action.command;
                result = std::move(action);
                break;
            }
        }
    }
    co_return result;
}

kota::task<> HandlerChain::on_error(data::ipcid_t id,
                                    std::optional<data::ipcid_t> parent,
                                    std::string message) {
    for(auto* handler: handlers) {
        co_await handler->on_error(id, parent, message);
    }
}

kota::task<> HandlerChain::on_exit(data::ipcid_t id, data::process_result result) {
    for(auto* handler: handlers) {
        co_await handler->on_exit(id, result);
    }
}

kota::task<> HandlerChain::on_finish(data::process_result result) {
    for(auto* handler: handlers) {
        co_await handler->on_finish(result);
    }
}

}  // namespace catter
