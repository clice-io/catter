#pragma once
#include <cstring>
#include <ranges>
#include <string>
#include <vector>
#include <type_traits>

#include <eventide/task.h>

namespace catter {

template <typename T>
concept Reader = std::is_invocable_v<T, char*, size_t>;

template <typename T>
concept CoReader = requires(T t, char* dst, size_t len) {
    { t(dst, len) };
    { t(dst, len).operator co_await() };
};

template <typename Range, typename T>
    requires std::ranges::range<std::decay_t<Range>> &&
             std::is_same_v<T, std::ranges::range_value_t<Range>>
void append_range_to_vector(std::vector<T>& buffer, Range&& range) {
#ifdef __cpp_lib_containers_ranges
    buffer.append_range(std::forward<Range>(range));
#else
    buffer.insert(buffer.end(), std::ranges::begin(range), std::ranges::end(range));
#endif
}

template <typename... Args,
          typename T = std::common_type_t<std::ranges::range_value_t<std::decay_t<Args>>...>>
    requires (std::ranges::range<std::decay_t<Args>> && ...)
std::vector<T> merge_range_to_vector(Args&&... args) {
    std::vector<T> buffer;
    buffer.reserve((std::ranges::size(args) + ...));
    (append_range_to_vector(buffer, std::forward<Args>(args)), ...);
    return buffer;
}

template <typename T>
struct Serde {
    static_assert("Not implemented");
};

template <typename T>
    requires std::is_integral_v<T>
struct Serde<T> {
    static std::vector<char> serialize(const T& value) {
        std::vector<char> buffer(sizeof(T));
        std::memcpy(buffer.data(), &value, sizeof(T));
        return buffer;
    }

    template <Reader Invocable>
    static T deserialize(Invocable&& reader) {
        T value;
        reader(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }

    template <CoReader Invocable>
    static eventide::task<T> co_deserialize(Invocable&& reader) {
        T value;
        co_await reader(reinterpret_cast<char*>(&value), sizeof(T));
        co_return value;
    }
};

template <>
struct Serde<std::string> {
    static std::vector<char> serialize(const std::string& str) {
        return merge_range_to_vector(Serde<size_t>::serialize(str.size()), str);
    }

    template <Reader Invocable>
    static std::string deserialize(Invocable&& reader) {
        size_t len = Serde<size_t>::deserialize(std::forward<Invocable>(reader));
        std::string str(len, '\0');
        reader(str.data(), len);
        return str;
    }

    template <CoReader Invocable>
    static eventide::task<std::string> co_deserialize(Invocable&& reader) {
        size_t len = co_await Serde<size_t>::co_deserialize(std::forward<Invocable>(reader));
        std::string str(len, '\0');
        co_await reader(str.data(), len);
        co_return str;
    }
};

template <typename T>
struct Serde<std::vector<T>> {
    static std::vector<char> serialize(const std::vector<T>& vec) {
        std::vector<char> buffer;
        append_range_to_vector(buffer, Serde<size_t>::serialize(vec.size()));
        for(const auto& item: vec) {
            append_range_to_vector(buffer, Serde<T>::serialize(item));
        }
        return buffer;
    }

    template <Reader Invocable>
    static std::vector<T> deserialize(Invocable&& reader) {
        std::vector<T> vec;
        size_t len = Serde<size_t>::deserialize(std::forward<Invocable>(reader));
        vec.reserve(len);

        for(size_t i = 0; i < len; ++i) {
            vec.push_back(Serde<T>::deserialize(std::forward<Invocable>(reader)));
        }
        return vec;
    }

    template <CoReader Invocable>
    static eventide::task<std::vector<T>> co_deserialize(Invocable&& reader) {
        std::vector<T> vec;
        size_t len = co_await Serde<size_t>::co_deserialize(std::forward<Invocable>(reader));
        vec.reserve(len);
        for(size_t i = 0; i < len; ++i) {
            vec.push_back(co_await Serde<T>::co_deserialize(std::forward<Invocable>(reader)));
        }
        co_return vec;
    }
};
};  // namespace catter
