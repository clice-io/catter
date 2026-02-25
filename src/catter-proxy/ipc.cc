#include <cstddef>
#include <print>
#include <span>
#include <stdexcept>

#include <eventide/async/loop.h>
#include <eventide/async/stream.h>

#include "config/ipc.h"
#include "util/data.h"
#include "util/eventide.h"

namespace catter::proxy::ipc {

class Impl {
public:
    Impl() noexcept {
        auto ret = wait(eventide::pipe::connect(config::ipc::PIPE_NAME,
                                                eventide::pipe::options(),
                                                default_loop()));
        if(!ret) {
            std::println("pipe connect failed: {}", ret.error().message());
            std::terminate();
        }
        this->client_pipe = std::move(ret.value());
    };

    Impl(const Impl&) = delete;
    Impl& operator= (const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator= (Impl&&) = delete;
    ~Impl() = default;

    auto reader() {
        return [this](char* dst, size_t len) {
            this->read(dst, len);
        };
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

public:
    eventide::pipe client_pipe{};
};

static Impl& impl() noexcept {
    static Impl instance;
    return instance;
}

void set_service_mode(data::ServiceMode mode) {
    impl().write(Serde<data::ServiceMode>::serialize(mode));
}

data::ipcid_t create(data::ipcid_t parent_id) {

    impl().write(Serde<data::Request>::serialize(data::Request::CREATE),
                 Serde<data::ipcid_t>::serialize(parent_id));
    return Serde<data::ipcid_t>::deserialize(impl().reader());
}

data::action make_decision(data::command cmd) {
    impl().write(Serde<data::Request>::serialize(data::Request::MAKE_DECISION),
                 Serde<data::command>::serialize(cmd));

    return Serde<data::action>::deserialize(impl().reader());
}

void finish(int64_t ret_code) {
    impl().write(Serde<data::Request>::serialize(data::Request::FINISH),
                 Serde<int64_t>::serialize(ret_code));
    return;
}

void report_error(data::ipcid_t parent_id, std::string error_msg) noexcept {
    try {
        impl().write(Serde<data::Request>::serialize(data::Request::REPORT_ERROR),
                     Serde<data::ipcid_t>::serialize(parent_id),
                     Serde<std::string>::serialize(error_msg));
    } catch(...) {
        // cannot do anything here
    }
    return;
};

}  // namespace catter::proxy::ipc
