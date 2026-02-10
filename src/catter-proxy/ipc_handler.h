#pragma once
#include <cstddef>
#include <print>
#include <span>
#include <stdexcept>

#include <eventide/loop.h>
#include <eventide/stream.h>

#include "config/ipc.h"
#include "util/ipc-data.h"
#include "util/eventide.h"

namespace catter::proxy {
class ipc_handler {
public:
    ipc_handler(const ipc_handler&) = delete;
    ipc_handler& operator= (const ipc_handler&) = delete;
    ipc_handler(ipc_handler&&) = delete;
    ipc_handler& operator= (ipc_handler&&) = delete;

    static ipc_handler& instance() noexcept {
        static ipc_handler instance;
        return instance;
    }

    auto reader() {
        return [this](char* dst, size_t len) {
            this->read(dst, len);
        };
    }

    ipc::data::command_id_t create(ipc::data::command_id_t parent_id) {
        this->parent_id = parent_id;
        this->write(Serde<ipc::data::Request>::serialize(ipc::data::Request::CREATE),
                    Serde<ipc::data::command_id_t>::serialize(parent_id));
        auto nxt_id = Serde<ipc::data::command_id_t>::deserialize(this->reader());
        this->id = nxt_id;
        return nxt_id;
    }

    ipc::data::action make_decision(ipc::data::command cmd) {
        this->write(Serde<ipc::data::Request>::serialize(ipc::data::Request::MAKE_DECISION),
                    Serde<ipc::data::command>::serialize(cmd));

        return Serde<ipc::data::action>::deserialize(this->reader());
    }

    void finish(int ret_code) {
        this->write(Serde<ipc::data::Request>::serialize(ipc::data::Request::FINISH),
                    Serde<int>::serialize(ret_code));
        return;
    }

    void report_error(std::string error_msg) noexcept {
        try {
            this->write(Serde<ipc::data::Request>::serialize(ipc::data::Request::REPORT_ERROR),
                        Serde<ipc::data::command_id_t>::serialize(this->parent_id),
                        Serde<ipc::data::command_id_t>::serialize(this->id),
                        Serde<std::string>::serialize(error_msg));
        } catch(...) {
            // cannot do anything here
        }
        return;
    };

private:
    template <typename... Args>
    void write(Args&&... payload) {
        (this->write(std::forward<Args>(payload)), ...);
    }

    template <typename T>
    void write(T&& payload) {
        auto err = wait(this->client_pipe.write(std::forward<T>(payload)));
        if(err.has_error()) {
            throw std::runtime_error(std::format("ipc_handler write failed: {}", err.message()));
        }
    }

    void read(char* dst, size_t len) {
        size_t total_read = 0;
        while(total_read < len) {
            auto ret = wait(this->client_pipe.read_some({dst + total_read, len - total_read}));
            if(ret == 0) {
                throw std::runtime_error("ipc_handler read failed: EOF/invalid");
            }
            total_read += ret;
        }
    }

    ipc_handler() noexcept {
        auto ret = wait(eventide::pipe::connect(config::ipc::PIPE_NAME,
                                                eventide::pipe::options(),
                                                default_loop()));
        if(!ret) {
            std::println("pipe connect failed: {}", ret.error().message());
            std::terminate();
        }
        this->client_pipe = std::move(ret.value());
    };

    ~ipc_handler() = default;

private:
    ipc::data::command_id_t parent_id{-1};
    ipc::data::command_id_t id{-1};
    eventide::pipe client_pipe{};
};
}  // namespace catter::proxy
