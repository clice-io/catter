#pragma once
#include "librpc/data.h"
#include "librpc/function.h"

// TODO
namespace catter::proxy {
class rpc_handler {
public:
    rpc_handler(const rpc_handler&) = delete;
    rpc_handler& operator= (const rpc_handler&) = delete;
    rpc_handler(rpc_handler&&) = delete;
    rpc_handler& operator= (rpc_handler&&) = delete;

    static rpc_handler& instance() noexcept {
        static rpc_handler instance;
        return instance;
    }

    rpc::data::command_id_t new_id() const {
        return id;
    }

    rpc::data::action make_decision(rpc::data::command_id_t parent_id,
                                    const rpc::data::command& cmd) {
        auto res = rpc::server::make_decision(parent_id, cmd);
        id = res.nxt_cmd_id;
        return res.act;
    }

    void report_error(rpc::data::command_id_t parent_id, const std::string& msg) noexcept {
        return rpc::server::report_error(parent_id, msg);
    };

    void finish(int ret_code) {
        return rpc::server::finish(ret_code);
    }

private:
    rpc_handler() noexcept = default;

private:
    rpc::data::command_id_t id{-1};
};
}  // namespace catter::proxy
