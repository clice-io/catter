#pragma once
#include <cstdint>
#include <cstring>
#include <ranges>
#include <string>
#include <vector>
#include <type_traits>

namespace catter::rpc::data {

using command_id_t = int32_t;
using thread_id_t = int32_t;
using timestamp_t = uint64_t;

struct command {
    /// do not ensure that this is a file path, this may be the name in PATH env
    std::string executable{};
    std::vector<std::string> args{};
    std::vector<std::string> env{};
};

struct action {
    enum : uint8_t {
        DROP,    // Do not execute the command
        INJECT,  // Inject <catter-payload> into the command
        WRAP,    // Wrap the command execution, and return its exit code
    } type;

    command cmd;
};

struct decision_info {
    action act;
    command_id_t nxt_cmd_id;
};

enum class Request : uint8_t {
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
};

template <typename T>
concept Reader = std::is_invocable_v<T, char*, size_t>;

template <typename T>
concept CoReader = std::is_invocable_v<T, char*, size_t> &&
    requires (T t) {
        { operator co_await(t) };
    };


template<typename Range>
    requires std::ranges::range<std::decay_t<Range>> && std::is_same_v<char, std::ranges::range_value_t<Range>>
void append_range_to_vector(std::vector<char>& buffer, Range&& range) {
#ifdef __cpp_lib_containers_ranges 
    buffer.append_range(std::forward<Range>(range));
#else
    buffer.insert(buffer.end(), std::ranges::begin(range), std::ranges::end(range));
#endif
}

template<typename ... Args>
    requires ((std::ranges::range<std::decay_t<Args>> && std::is_same_v<char, std::ranges::range_value_t<Args>>) && ...)
std::vector<char> merge_range_to_vector(Args&& ... args) {
    std::vector<char> buffer;
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

    // template< CoReader Invocable>
    // static coro::Lazy<T> co_deserialize(Invocable&& reader) {
    //     T value;
    //     co_await reader(reinterpret_cast<char*>(&value), sizeof(T));
    //     co_return value;
    // }

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
};

template <>
struct Serde<Request> {
    static std::vector<char> serialize(const Request& req) {
        return Serde<uint8_t>::serialize(static_cast<uint8_t>(req));
    }

    template <Reader Invocable>
    static Request deserialize(Invocable&& reader) {
        uint8_t value = Serde<uint8_t>::deserialize(std::forward<Invocable>(reader));
        return static_cast<Request>(value);
    }
};

template <>
struct Serde<command> {
    static std::vector<char> serialize(const command& cmd) {
        return merge_range_to_vector(
                Serde<std::string>::serialize(cmd.executable),
                Serde<std::vector<std::string>>::serialize(cmd.args),
                Serde<std::vector<std::string>>::serialize(cmd.env)
            );
    }

    template <Reader Invocable>
    static command deserialize(Invocable&& reader) {
        command cmd;
        cmd.executable = Serde<std::string>::deserialize(std::forward<Invocable>(reader));
        cmd.args = Serde<std::vector<std::string>>::deserialize(std::forward<Invocable>(reader));
        cmd.env = Serde<std::vector<std::string>>::deserialize(std::forward<Invocable>(reader));
        return cmd;
    }
};

template <>
struct Serde<action> {
    static std::vector<char> serialize(const action& act) {
        return merge_range_to_vector(
            Serde<uint8_t>::serialize(static_cast<uint8_t>(act.type)),
            Serde<command>::serialize(act.cmd)
        );
    }

    template <Reader Invocable>
    static action deserialize(Invocable&& reader) {
        using enum_type = decltype(action::type);
        return {
            static_cast<enum_type>(Serde<uint8_t>::deserialize(std::forward<Invocable>(reader))),
            Serde<command>::deserialize(std::forward<Invocable>(reader))};
    }
};

template <>
struct Serde<decision_info> {
    static std::vector<char> serialize(const decision_info& info) {
        return merge_range_to_vector(
            Serde<action>::serialize(info.act),
            Serde<command_id_t>::serialize(info.nxt_cmd_id)
        );
    }

    template <Reader Invocable>
    static decision_info deserialize(Invocable&& reader) {
        return {Serde<action>::deserialize(std::forward<Invocable>(reader)),
                Serde<command_id_t>::deserialize(std::forward<Invocable>(reader))};
    }
};
}  // namespace catter::rpc::data
