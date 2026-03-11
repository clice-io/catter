#include <cassert>
#include <cstddef>
#include <optional>
#include <print>
#include <tuple>
#include <memory>
#include <format>
#include <utility>

#include <eventide/reflection/enum.h>
#include <eventide/common/functional.h>

#include "ipc.h"

#include "util/log.h"
#include "util/serde.h"
#include "util/data.h"

namespace catter::ipc {
using namespace data;

template <Request Req, typename T>
struct Helper {};

template <typename... Args>
auto deserialize_args(BufferReader& buf_reader) {
    return std::tuple<std::remove_cvref_t<Args>...>{Serde<Args>::deserialize(buf_reader)...};
}

template <Request Req, typename Ret, typename... Args>
struct Helper<Req, Ret(Args...)> {
    template <typename Writer>
    eventide::task<void> operator() (eventide::function_ref<Ret(Args...)> callback,
                                     BufferReader& buf_reader,
                                     Writer&& write_packet) {
        auto args = deserialize_args<Args...>(buf_reader);
        if constexpr(!std::is_same_v<Ret, void>) {
            auto ret =
                co_await write_packet(Serde<Ret>::serialize(std::apply(callback, std::move(args))));

            if(ret.has_error()) {
                throw std::runtime_error(std::format("Failed to send response [{}] to client: {}",
                                                     eventide::refl::enum_name(Req),
                                                     ret.message()));
            }
        } else {
            std::apply(callback, std::move(args));
            co_return;
        }
    }
};

template <Request Req, typename Writer>
eventide::task<void> handle_req(eventide::function_ref<RequestType<Req>> callback,
                                BufferReader& buf_reader,
                                Writer&& write_packet) {
    return Helper<Req, RequestType<Req>>{}(callback,
                                           buf_reader,
                                           std::forward<Writer>(write_packet));
}

eventide::task<void> accept(std::unique_ptr<InjectService> service, eventide::pipe client) {

    auto read_exact = [&](char* dst, size_t len, bool allow_eof = false) -> eventide::task<bool> {
        size_t total_read = 0;
        while(total_read < len) {
            auto ret = co_await client.read_some({dst + total_read, len - total_read});
            if(!ret) {
                throw std::runtime_error(
                    std::format("ipc_handler read failed: {}", ret.error().message()));
            }
            if(ret.value() == 0) {
                if(allow_eof && total_read == 0) {
                    co_return false;
                }

                throw std::runtime_error(
                    std::format("Client disconnected while reading packet ({} of {} bytes read)",
                                total_read,
                                len));
            }
            total_read += ret.value();
        }
        LOG_DEBUG("Reading {} bytes: {}", len, log::to_hex(std::span<char>(dst, len)));
        co_return true;
    };

    auto writer = [&](const std::vector<char>& payload) -> eventide::task<eventide::error> {
        LOG_DEBUG("Writing {} bytes: {}", payload.size(), log::to_hex(payload));
        return client.write(payload);
    };

    auto read_packet = [&]() -> eventide::task<std::optional<packet>> {
        size_t packet_size = 0;
        if(!(co_await read_exact(reinterpret_cast<char*>(&packet_size),
                                 sizeof(packet_size),
                                 true))) {
            co_return std::nullopt;
        }

        packet payload(packet_size);
        if(packet_size > 0) {
            co_await read_exact(payload.data(), packet_size);
        }

        co_return payload;
    };

    auto write_packet = [&](const std::vector<char>& payload) -> eventide::task<eventide::error> {
        co_return co_await writer(Serde<packet>::serialize(payload));
    };

    auto sm_packet = co_await read_packet();
    if(!sm_packet) {
        std::println("Client disconnected before sending service mode");
        co_return;
    }

    auto service_mode = Serde<ServiceMode>::deserialize(BufferReader(*sm_packet));
    assert(service_mode == ServiceMode::INJECT && "Unsupported service mode received");

    while(true) {
        auto req_packet = co_await read_packet();
        if(!req_packet) {
            std::println("Client disconnected");
            break;
        }

        BufferReader buf_reader(*req_packet);
        Request req = Serde<Request>::deserialize(buf_reader);
        switch(req) {
            case Request::CREATE: {
                co_await handle_req<Request::CREATE>(
                    eventide::bind_ref<&InjectService::create>(*service),
                    buf_reader,
                    write_packet);
                break;
            }

            case Request::MAKE_DECISION: {
                co_await handle_req<Request::MAKE_DECISION>(
                    eventide::bind_ref<&InjectService::make_decision>(*service),
                    buf_reader,
                    write_packet);
                break;
            }
            case Request::FINISH: {
                co_await handle_req<Request::FINISH>(
                    eventide::bind_ref<&InjectService::finish>(*service),
                    buf_reader,
                    write_packet);
                break;
            }
            case Request::REPORT_ERROR: {
                co_await handle_req<Request::REPORT_ERROR>(
                    eventide::bind_ref<&InjectService::report_error>(*service),
                    buf_reader,
                    write_packet);
                break;
            }
            default: {
                assert(false && "Unknown request type received");
            }
        }
    }

    co_return;
}

}  // namespace catter::ipc
