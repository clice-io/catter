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

    rpc::data::command_id_t new_id() const {
        return id;
    }

    rpc::data::action make_decision(rpc::data::command_id_t parent_id, rpc::data::command cmd) {
        this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::MAKE_DECISION),
              Serde<rpc::data::command_id_t>::serialize(parent_id),
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

    void report_error(rpc::data::command_id_t parent_id, std::string error_msg) noexcept {
        try {
            this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::REPORT_ERROR),
                Serde<rpc::data::command_id_t>::serialize(parent_id),
                Serde<std::string>::serialize(error_msg));
        } catch(...) {
            // cannot do anything here
        }
        return;
    };

private:
    template <typename... Args>
    void write(Args&&... payload) {
        uv::wait([&]() -> coro::Lazy<void> {
            co_await uv::async::write(uv::cast<uv_stream_t>(&this->client_pipe),
                                      std::forward<Args>(payload)...);
            co_return;
        }());
    }

    void read(char* dst, size_t len) {
        uv::wait([&]() -> coro::Lazy<void> {
            co_await uv::async::read(uv::cast<uv_stream_t>(&this->client_pipe), dst, len);
            co_return;
        }());
    }

    rpc_handler() noexcept {
        uv_pipe_init(uv::default_loop(), &this->client_pipe, 0);
        uv_connect_t connect_req{};
        uv_pipe_connect(&connect_req,
                        &this->client_pipe,
                        catter::config::rpc::PIPE_NAME,
                        [](uv_connect_t* /*req*/, int status) {
                            if (status < 0) {
                                std::println("Failed to connect to parent process: {}", uv_strerror(status));
                            }
                        });
        
        uv::run();
    };

    ~rpc_handler() {
        uv_close(uv::cast<uv_handle_t>(&this->client_pipe), nullptr);
        uv::run();
    };

private:
    rpc::data::command_id_t id{-1};
    uv_pipe_t client_pipe{};
};
}  // namespace catter::proxy
