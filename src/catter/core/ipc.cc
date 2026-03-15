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
#include "util/packet_io.h"

namespace catter::ipc {
using namespace data;

template <typename T>
struct Helper;

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
        if constexpr(idx == static_cast<size_t>(-1)) {
            return nullptr;
        } else {
            return std::get<idx>(Tuple);
        }
    }

    template <typename T>
    struct Helper;

    template <typename Ret, typename... Args>
    struct Helper<Ret(Args...)> {
        static std::optional<std::vector<char>> call(eventide::function_ref<Ret(Args...)> callback,
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
        using ReflRequest = eventide::refl::reflection<Request>;
        Request req = Serde<Request>::deserialize(buf_reader);
        LOG_INFO("Handling request of type: {}", eventide::refl::enum_name(req));
        return [&]<size_t I = 0>(this const auto& self) -> std::optional<std::vector<char>> {
            if constexpr(I < ReflRequest::member_count) {
                constexpr auto val = ReflRequest::member_values[I];
                if(val == req) {
                    constexpr auto mem_fn = match_mem_fn<val>();
                    if constexpr(mem_fn == nullptr) {
                        LOG_INFO("No matching member function found for request type: {}",
                                 eventide::refl::enum_name(req));
                        throw std::runtime_error(
                            std::format("No matching member function found for request type: {}",
                                        eventide::refl::enum_name(req)));
                    } else {
                        return Helper<RequestType<val>>::call(eventide::bind_ref<mem_fn>(obj),
                                                              buf_reader);
                    }
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
    PacketChannel channel(std::move(client));

    auto sm_packet = co_await channel.read_packet();
    if(!sm_packet) {
        std::println("Client disconnected before sending service mode");
        co_return;
    }

    auto service_mode = Serde<ServiceMode>::deserialize(BufferReader(*sm_packet));
    assert(service_mode == ServiceMode::INJECT && "Unsupported service mode received");

    while(true) {
        auto req_packet = co_await channel.read_packet();
        if(!req_packet) {
            std::println("Client disconnected");
            break;
        }

        BufferReader buf_reader(*req_packet);

        using DispatcherType = Dispatcher<&InjectService::create,
                                          &InjectService::make_decision,
                                          &InjectService::finish,
                                          &InjectService::report_error>;

        auto response = DispatcherType::dispatch(*service, buf_reader);
        if(response.has_value()) {
            co_await channel.write_packet(*response);
        }
    }
    co_return;
}

}  // namespace catter::ipc
