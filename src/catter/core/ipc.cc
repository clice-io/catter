#include <cassert>
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
                                                     eventide::refl::enum_name<Req>(),
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

eventide::task<void> accept(std::unique_ptr<DefaultService> service, eventide::pipe client) {

    auto reader = [&](char* dst, size_t len) -> eventide::task<void> {
        size_t total_read = 0;
        while(total_read < len) {
            auto ret = co_await client.read_some({dst + total_read, len - total_read});
            if(!ret) {
                throw std::runtime_error(
                    std::format("ipc_handler read failed: {}", ret.error().message()));
            }
            if(ret.value() == 0) {
                throw ret.value();  // EOF or client disconnected
            }
            total_read += ret.value();
        }
        LOG_DEBUG("Reading {} bytes: {}", len, log::to_hex(std::span<char>(dst, len)));
        co_return;
    };

    auto writer = [&](const std::vector<char>& payload) -> eventide::task<eventide::error> {
        LOG_DEBUG("Writing {} bytes: {}", payload.size(), log::to_hex(payload));
        return client.write(payload);
    };

    auto read_packet = [&]() -> eventide::task<packet> {
        co_return co_await Serde<packet>::co_deserialize(reader);
    };

    auto write_packet = [&](const std::vector<char>& payload) -> eventide::task<eventide::error> {
        co_return co_await writer(Serde<packet>::serialize(payload));
    };

    try {
        auto sm_packet = co_await read_packet();
        auto service_mode = Serde<ServiceMode>::deserialize(BufferReader(sm_packet));
        assert(service_mode == ServiceMode::DEFAULT && "Unsupported service mode received");
        while(true) {
            auto req_packet = co_await read_packet();
            BufferReader buf_reader(req_packet);
            Request req = Serde<Request>::deserialize(buf_reader);
            switch(req) {
                case Request::CREATE: {
                    co_await handle_req<Request::CREATE>(
                        eventide::bind_ref<&DefaultService::create>(*service),
                        buf_reader,
                        write_packet);
                    break;
                }

                case Request::MAKE_DECISION: {
                    co_await handle_req<Request::MAKE_DECISION>(
                        eventide::bind_ref<&DefaultService::make_decision>(*service),
                        buf_reader,
                        write_packet);
                    break;
                }
                case Request::FINISH: {
                    co_await handle_req<Request::FINISH>(
                        eventide::bind_ref<&DefaultService::finish>(*service),
                        buf_reader,
                        write_packet);
                    break;
                }
                case Request::REPORT_ERROR: {
                    co_await handle_req<Request::REPORT_ERROR>(
                        eventide::bind_ref<&DefaultService::report_error>(*service),
                        buf_reader,
                        write_packet);
                    break;
                }
                default: {
                    assert(false && "Unknown request type received");
                }
            }
        }
    } catch(size_t err) {
        // EOF or client disconnected
        assert(err == 0 && "Unexpected error in IPC communication");
    }
    co_return;
}

}  // namespace catter::ipc
