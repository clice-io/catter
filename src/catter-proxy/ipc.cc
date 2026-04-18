#include "config/ipc.h"

#include <cstddef>
#include <print>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <kota/async/async.h>

#include "util/data.h"
#include "util/kotatsu.h"
#include "util/log.h"
#include "util/packet_io.h"

namespace catter::proxy::ipc {
using namespace data;

class Impl {
public:
    Impl() noexcept {
        auto ret = wait(
            kota::pipe::connect(config::ipc::pipe_name(), kota::pipe::options(), default_loop()));
        if(!ret) {
            std::println("pipe connect failed: {}", ret.error().message());
            std::terminate();
        }
        this->channel = PacketChannel(std::move(ret.value()));
    };

    Impl(const Impl&) = delete;
    Impl& operator= (const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator= (Impl&&) = delete;
    ~Impl() = default;

private:
    void write_packet_checked(const std::vector<char>& payload) {
        auto err = wait(channel.write_packet(payload));
        if(err.has_error()) {
            throw std::runtime_error(
                std::format("ipc_handler write failed: {}", err.error().message()));
        }
    }

public:
    static Impl& instance() noexcept {
        static Impl instance;
        return instance;
    }

    void write_packet(const std::vector<char>& payload) {
        write_packet_checked(payload);
    }

    packet read_packet() {
        auto result = wait(channel.read_packet());
        if(!result) {
            throw std::runtime_error("ipc_handler read failed: EOF");
        }
        return std::move(*result);
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
    PacketChannel channel{};
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

void finish(process_result result) {
    Impl::request<Request::FINISH>(std::move(result));
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
