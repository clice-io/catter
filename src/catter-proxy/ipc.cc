#include <cstddef>
#include <print>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <eventide/async/loop.h>
#include <eventide/async/stream.h>

#include "config/ipc.h"
#include "util/log.h"
#include "util/data.h"
#include "util/eventide.h"

namespace catter::proxy::ipc {
using namespace data;

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

private:
    void read(char* dst, size_t len) {
        size_t total_read = 0;
        while(total_read < len) {
            auto ret = wait(this->client_pipe.read_some({dst + total_read, len - total_read}));
            if(!ret) {
                throw std::runtime_error(
                    std::format("ipc_handler read failed: {}", ret.error().message()));
            }
            total_read += ret.value();
        }
        LOG_DEBUG("Reading {} bytes: {}", len, log::to_hex(std::span<char>(dst, len)));
    }

    void write(const std::vector<char>& payload) {
        LOG_DEBUG("Writing {} bytes: {}", payload.size(), log::to_hex(payload));
        auto err = wait(this->client_pipe.write(payload));
        if(err.has_error()) {
            throw std::runtime_error(std::format("ipc_handler write failed: {}", err.message()));
        }
    }

    auto reader() {
        return [this](char* dst, size_t len) {
            this->read(dst, len);
        };
    }

public:
    static Impl& instance() noexcept {
        static Impl instance;
        return instance;
    }

    void write_packet(const std::vector<char>& payload) {
        this->write(Serde<packet>::serialize(payload));
    }

    packet read_packet() {
        return Serde<packet>::deserialize(reader());
    }

    template <typename T>
    struct request_helper {};

    template <typename Ret, typename... Args>
    struct request_helper<Ret(Args...)> {
        using type = Ret;
    };

    template <Request Req, typename... Args>
    static auto serialize_request(Args&&... args) {
        auto payload = Serde<Request>::serialize(Req);
        (append_range_to_vector(
             payload,
             Serde<std::remove_cvref_t<Args>>::serialize(std::forward<Args>(args))),
         ...);
        return payload;
    }

    template <Request Req, typename... Args>
    static auto request(Args&&... args) {
        using Ret = typename request_helper<RequestType<Req>>::type;

        auto payload = serialize_request<Req>(std::forward<Args>(args)...);
        instance().write_packet(payload);

        if constexpr(!std::is_same_v<Ret, void>) {
            auto packet = instance().read_packet();
            BufferReader buf_reader(packet);
            return Serde<Ret>::deserialize(buf_reader);
        }
    }

    static void set_service_mode(ServiceMode mode) {
        instance().write_packet(Serde<ServiceMode>::serialize(mode));
    }

private:
    eventide::pipe client_pipe{};
};

void set_service_mode(ServiceMode mode) {
    Impl::set_service_mode(mode);
}

ipcid_t create(ipcid_t parent_id) {
    return Impl::request<Request::CREATE>(parent_id);
}

action make_decision(command cmd) {
    return Impl::request<Request::MAKE_DECISION>(cmd);
}

void finish(int64_t ret_code) {
    Impl::request<Request::FINISH>(ret_code);
}

void report_error(ipcid_t parent_id, std::string error_msg) noexcept {
    try {
        Impl::request<Request::REPORT_ERROR>(parent_id, error_msg);
    } catch(...) {
        // cannot do anything here
    }
    return;
};

}  // namespace catter::proxy::ipc
