#include <cassert>
#include <cstddef>
#include <new>
#include <optional>
#include <print>
#include <tuple>
#include <memory>
#include <format>
#include <type_traits>
#include <utility>

#include <eventide/reflection/enum.h>
#include <eventide/common/functional.h>
#include <eventide/common/meta.h>
#include <vector>

#include "ipc.h"

#include "util/log.h"
#include "util/serde.h"
#include "util/data.h"

namespace catter::ipc {
using namespace data;

template <typename T>
struct Helper;

template <typename... Args>
auto deserialize_args(BufferReader& buf_reader) {
    return std::tuple<Args...>{Serde<Args>::deserialize(buf_reader)...};
}

template <typename Ret, typename... Args>
struct Helper<Ret(Args...)> {
    static auto read(BufferReader& buf_reader) {
        return deserialize_args<Args...>(buf_reader);
    }
};

template <Request Req>
eventide::task<void> handle_req(BufferReader& buf_reader) {
    return Helper<RequestType<Req>>::handle(buf_reader);
}

template <auto... MemFns>
struct Dispatcher {
    using Class = std::common_type_t<typename eventide::mem_fn<MemFns>::ClassType...>;

    constexpr static auto Tuple = std::make_tuple(MemFns...);

    template <Request Req>
    static consteval auto match_mem_fn() {
        constexpr auto idx = []<size_t I>(this auto self, std::in_place_index_t<I>) {
            if constexpr(I < sizeof...(MemFns)) {
                using Fn = eventide::mem_fn<std::get<I>(Tuple)>::FunctionType;
                if constexpr(std::same_as<Fn, RequestType<Req>>) {
                    return I;
                } else {
                    return self(std::in_place_index<I + 1>);
                }
            } else {
                return static_cast<size_t>(-1);
            }
        }(std::in_place_index<0>);
        static_assert(idx != static_cast<size_t>(-1),
                      "No matching request type found in dispatcher");
        return std::get<idx>(Tuple);
    }

    template <typename T>
    struct Helper;

    template <typename Ret, typename... Args>
    struct Helper<Ret(Args...)> {
        static std::optional<std::vector<char>> call(eventide::function_ref<Ret(Args...)> callback,
                                                     BufferReader& buf_reader) {
            auto args_tuple = deserialize_args<Args...>(buf_reader);
            if constexpr(std::is_void_v<Ret>) {
                std::apply(callback, args_tuple);
                return std::nullopt;
            } else {
                Ret result = std::apply(callback, args_tuple);
                return Serde<Ret>::serialize(result);
            }
        }
    };

    static std::optional<std::vector<char>> dispatch(Class& obj, BufferReader& buf_reader) {
        using ReflRequest = eventide::refl::reflection<Request>;
        Request req = Serde<Request>::deserialize(buf_reader);

        return [&]<size_t I = 0>(this const auto& self) -> std::optional<std::vector<char>> {
            if constexpr(I < ReflRequest::member_count) {
                constexpr auto val = ReflRequest::member_values[I];
                if(val == req) {
                    return Helper<RequestType<val>>::call(
                        eventide::bind_ref<match_mem_fn<val>()>(obj),
                        buf_reader);
                }
                return self.template operator()<I + 1>();
            } else {
                throw std::runtime_error(
                    std::format("Unknown request type received: {}",
                                static_cast<std::underlying_type_t<Request>>(req)));
            }
        }();
    }
};

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

        using DispatcherType = Dispatcher<&InjectService::create,
                                          &InjectService::make_decision,
                                          &InjectService::finish,
                                          &InjectService::report_error>;

        auto response = DispatcherType::dispatch(*service, buf_reader);
        if(response.has_value()) {
            co_await write_packet(*response);
        }
    }
    co_return;
}

}  // namespace catter::ipc
