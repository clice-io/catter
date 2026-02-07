#pragma once
#include <print>
#include <span>
#include <stdexcept>

#include <eventide/loop.h>
#include <eventide/stream.h>

#include "config/rpc.h"
#include "uv/rpc_data.h"

inline auto& default_loop() noexcept {
    static eventide::event_loop loop{};
    return loop;
}

template <typename Task>
auto wait(Task&& task) {
    default_loop().schedule(task);
    default_loop().run();
    return task.result();
}

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

    auto reader() {
        return [this](char* dst, size_t len) {
            read(dst, len);
        };
    }

    rpc::data::command_id_t create(rpc::data::command_id_t parent_id) {
        this->parent_id = parent_id;
        this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::CREATE),
                    Serde<rpc::data::command_id_t>::serialize(parent_id));
        auto nxt_id = Serde<rpc::data::command_id_t>::deserialize(this->reader());
        this->id = nxt_id;
        return nxt_id;
    }

    rpc::data::action make_decision(rpc::data::command cmd) {
        this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::MAKE_DECISION),
                    Serde<rpc::data::command>::serialize(cmd));

        return Serde<rpc::data::action>::deserialize(this->reader());
    }

    void finish(int ret_code) {
        this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::FINISH),
                    Serde<int>::serialize(ret_code));
        return;
    }

    void report_error(std::string error_msg) noexcept {
        try {
            this->write(Serde<rpc::data::Request>::serialize(rpc::data::Request::REPORT_ERROR),
                        Serde<rpc::data::command_id_t>::serialize(this->parent_id),
                        Serde<rpc::data::command_id_t>::serialize(this->id),
                        Serde<std::string>::serialize(error_msg));
        } catch(...) {
            // cannot do anything here
        }
        return;
    };

private:
    template <typename... Args>
    void write(Args&&... payload) {
        //TODO
        (::wait(this->client_pipe.write(std::forward<Args>(payload))), ...);
    }

    void read(char* dst, size_t len) {
        auto ret = ::wait(this->client_pipe.read_some({dst, len}));

        if(ret == 0) {
            throw std::runtime_error("rpc_handler read failed: EOF/invalid");
        }
    }

    rpc_handler() noexcept {
        auto ret = ::wait(eventide::pipe::connect(config::rpc::PIPE_NAME, eventide::pipe::options(), default_loop()));
        if(!ret) {
            std::println("pipe connect failed: {}", ret.error().message());
            std::terminate();
        }
        this->client_pipe = std::move(ret.value());
    };

    ~rpc_handler() = default;

private:
    rpc::data::command_id_t parent_id{-1};
    rpc::data::command_id_t id{-1};
    eventide::pipe client_pipe{};
};
}  // namespace catter::proxy
