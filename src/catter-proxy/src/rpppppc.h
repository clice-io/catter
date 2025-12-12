#pragma once
#include "libutil/rpc_data.h"
#include "libutil/uv.h"
#include "libconfig/rpc.h"
#include <print>
#include <uv.h>

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

    rpc::data::command_id_t get_id() const {
        return id;
    }

    void set_parent_id(rpc::data::command_id_t pid) {
        this->parent_id = pid;
    }

    rpc::data::action make_decision(rpc::data::command cmd) {
        this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::MAKE_DECISION),
                    Serde<rpc::data::command_id_t>::serialize(this->parent_id),
                    Serde<rpc::data::command>::serialize(cmd));

        auto [act, nxt_cmd_id] = Serde<rpc::data::decision_info>::deserialize(
            [this](char* dst, size_t len) { read(dst, len); });

        this->id = nxt_cmd_id;
        return act;
    }

    void finish(int ret_code) {
        this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::FINISH),
                    Serde<int>::serialize(ret_code));
        return;
    }

    void report_error(std::string error_msg) noexcept {
        try {
            auto to_send = this->id;
            if (to_send == -1) {
                to_send = this->parent_id;
            }
            this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::REPORT_ERROR),
                        Serde<rpc::data::command_id_t>::serialize(to_send),
                        Serde<std::string>::serialize(error_msg));
        } catch(...) {
            // cannot do anything here
        }
        return;
    };

private:
    template <typename... Args>
    void write(Args&&... payload) {
        uv::wait(uv::async::write(uv::cast<uv_stream_t>(&this->client_pipe),
                                  std::forward<Args>(payload)...));
    }

    void read(char* dst, size_t len) {
        uv::wait(uv::async::read(uv::cast<uv_stream_t>(&this->client_pipe), dst, len));
    }

    rpc_handler() noexcept {
        uv_pipe_init(uv::default_loop(), &this->client_pipe, 0);
        uv_connect_t connect_req{};
        uv_pipe_connect(&connect_req,
                        &this->client_pipe,
                        config::rpc::PIPE_NAME,
                        nullptr);

        uv::run();
    };

    ~rpc_handler() {
        uv_close(uv::cast<uv_handle_t>(&this->client_pipe), nullptr);
        uv::run();
    };

private:
    rpc::data::command_id_t parent_id{-1};
    rpc::data::command_id_t id{-1};
    uv_pipe_t client_pipe{};
};
}  // namespace catter::proxy
