#include "ipc.h"

#include <cassert>
#include <cstddef>
#include <format>
#include <memory>
#include <new>
#include <optional>
#include <print>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "util/data.h"
#include "util/enum.h"
#include "util/log.h"
#include "util/packet_io.h"
#include "util/serde.h"

#include <kota/meta/enum.h>
#include <kota/support/functional.h>
#include <kota/support/type_traits.h>

namespace catter::ipc {
using namespace data;

template <typename T>
struct Helper;

template <auto... MemFns>
struct Dispatcher {
    using Class = std::common_type_t<typename kota::mem_fn<MemFns>::ClassType...>;

    template <size_t I>
    constexpr static auto get() {
        return std::get<I>(std::make_tuple(MemFns...));
    }

    template <Request Req>
    static consteval auto match_mem_fn() {
        constexpr auto idx = []<size_t I>(this auto self, std::in_place_index_t<I>) {
            if constexpr(I < sizeof...(MemFns)) {
                using Fn = kota::mem_fn<get<I>()>::FunctionType;
                if constexpr(std::same_as<Fn, RequestType<Req>>) {
                    return I;
                } else {
                    return self(std::in_place_index<I + 1>);
                }
            } else {
                return static_cast<size_t>(-1);
            }
        }(std::in_place_index<0>);
        if constexpr(idx == static_cast<size_t>(-1)) {
            return nullptr;
        } else {
            return get<idx>();
        }
    }

    template <typename T>
    struct Helper;

    template <typename Ret, typename... Args>
    struct Helper<Ret(Args...)> {
        static std::optional<std::vector<char>> call(kota::function_ref<Ret(Args...)> callback,
                                                     BufferReader& buf_reader) {
            auto args_tuple = std::tuple<Args...>{Serde<Args>::deserialize(buf_reader)...};
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
        Request req = Serde<Request>::deserialize(buf_reader);
        LOG_INFO("Handling request of type: {}", kota::meta::enum_name(req));

        return catter::dispatch(
            req,
            [&]<auto E>(in_place_enum<E>) -> std::optional<std::vector<char>> {
                constexpr auto mem_fn = match_mem_fn<E>();
                if constexpr(mem_fn == nullptr) {
                    LOG_INFO("No matching member function found for request type: {}",
                             kota::meta::enum_name(req));
                    throw std::runtime_error(
                        std::format("No matching member function found for request type: {}",
                                    kota::meta::enum_name(req)));
                } else {
                    return Helper<RequestType<E>>::call(kota::bind_ref<mem_fn>(obj), buf_reader);
                }
            });
    }
};

kota::task<void> accept(std::unique_ptr<InjectService> service, kota::pipe client) {
    PacketChannel channel(std::move(client));

    auto sm_packet = co_await channel.read_packet();
    if(!sm_packet) {
        LOG_INFO("Client disconnected before sending service mode");
        co_return;
    }

    auto service_mode = Serde<ServiceMode>::deserialize(BufferReader(*sm_packet));
    assert(service_mode == ServiceMode::INJECT && "Unsupported service mode received");

    while(true) {
        auto req_packet = co_await channel.read_packet();
        if(!req_packet) {
            LOG_INFO("Client disconnected");
            break;
        }

        BufferReader buf_reader(*req_packet);

        using DispatcherType = Dispatcher<&InjectService::create,
                                          &InjectService::make_decision,
                                          &InjectService::finish,
                                          &InjectService::report_error>;

        auto response = DispatcherType::dispatch(*service, buf_reader);
        if(response.has_value()) {
            if((co_await channel.write_packet(*response)).has_error()) {
                LOG_INFO("Failed to write response packet");
                break;
            }
        }
    }
    co_return;
}

}  // namespace catter::ipc
