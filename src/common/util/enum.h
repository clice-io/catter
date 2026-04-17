#pragma once
#include <array>
#include <cstddef>
#include <fcntl.h>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include <kota/meta/enum.h>
#include <kota/meta/struct.h>

namespace catter {

template <auto E>
    requires std::is_enum_v<std::decay_t<decltype(E)>>
struct in_place_enum {
    using type = std::decay_t<decltype(E)>;
    constexpr static type value = E;
};

template <typename E, typename F>
    requires std::is_enum_v<E> && std::is_invocable_v<F, in_place_enum<E{}>>
auto dispatch(E e, F&& f) {
    using Enum = kota::meta::reflection<E>;
    using R = std::invoke_result_t<F, in_place_enum<E{}>>;
    using Callback = R(F && f);

    struct Data {
        E value;
        Callback* callback;
    };

    constexpr auto map = []<size_t... I>(std::index_sequence<I...>) {
        return std::to_array<Data>({
            {Enum::member_values[I], [](F&& f) -> R {
                 return std::invoke(std::forward<F>(f), in_place_enum<Enum::member_values[I]>{});
             }}
            ...
        });
    }(std::make_index_sequence<Enum::member_count>{});
    using U = std::underlying_type_t<E>;
    const auto target = static_cast<U>(e);
    std::size_t left = 0;
    std::size_t right = Enum::member_values.size();
    while(left < right) {
        const auto mid = left + (right - left) / 2;
        if(static_cast<U>(Enum::member_values[mid]) < target) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    if(left < Enum::member_values.size() && static_cast<U>(Enum::member_values[left]) == target) {
        return map[left].callback(std::forward<F>(f));
    }
    throw std::runtime_error("Invalid enum value");
}

template <typename E, typename F>
    requires std::is_enum_v<E> && std::is_invocable_v<F, in_place_enum<E{}>>
auto dispatch(std::string_view e, F&& f) {
    using Enum = kota::meta::reflection<E>;
    using R = std::invoke_result_t<F, in_place_enum<E{}>>;
    using Callback = R(F && f);

    struct Data {
        std::string_view name;
        Callback* callback;
    };

    constexpr auto map = []<size_t... I>(std::index_sequence<I...>) {
        return std::to_array<Data>({
            {Enum::member_names[I], [](F&& f) -> R {
                 return std::invoke(std::forward<F>(f), in_place_enum<Enum::member_values[I]>{});
             }}
            ...
        });
    }(std::make_index_sequence<Enum::member_count>{});

    for(const auto& entry: map) {
        if(entry.name == e) {
            return entry.callback(std::forward<F>(f));
        }
    }
    throw std::runtime_error("Invalid enum name");
}
}  // namespace catter
