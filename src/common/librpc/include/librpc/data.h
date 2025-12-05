#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>
#include <type_traits>
#include <memory>
#include <variant>
#include <stdexcept>

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
concept Reader = std::is_invocable_r_v<void, T, char*, size_t>;

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

    template <typename Invocable>
        requires std::invocable<Invocable, char*, size_t>
    static T deserialize(Invocable&& reader) {
        T value;
        reader(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }
};

template <>
struct Serde<std::string> {
    static std::vector<char> serialize(const std::string& str) {
        std::vector<char> buffer{};
        buffer.append_range(Serde<size_t>::serialize(str.size()));
        buffer.append_range(str);
        return buffer;
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
        buffer.append_range(Serde<size_t>::serialize(vec.size()));
        for(const auto& item: vec) {
            buffer.append_range(Serde<T>::serialize(item));
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
        std::vector<char> buffer;
        buffer.append_range(Serde<std::string>::serialize(cmd.executable));
        buffer.append_range(Serde<std::vector<std::string>>::serialize(cmd.args));
        buffer.append_range(Serde<std::vector<std::string>>::serialize(cmd.env));
        return buffer;
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
        std::vector<char> buffer;
        buffer.append_range(Serde<uint8_t>::serialize(static_cast<uint8_t>(act.type)));
        buffer.append_range(Serde<command>::serialize(act.cmd));
        return buffer;
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
        std::vector<char> buffer;
        buffer.append_range(Serde<action>::serialize(info.act));
        buffer.append_range(Serde<command_id_t>::serialize(info.nxt_cmd_id));
        return buffer;
    }

    template <Reader Invocable>
    static decision_info deserialize(Invocable&& reader) {
        return {Serde<action>::deserialize(std::forward<Invocable>(reader)),
                Serde<command_id_t>::deserialize(std::forward<Invocable>(reader))};
    }
};

template <typename... Args>
struct Serde<std::variant<Args...>> {
    static std::vector<char> serialize(const std::variant<Args...>& var) {
        std::vector<char> buffer;
        buffer.append_range(Serde<size_t>::serialize(var.index()));
        buffer.append_range(std::visit(
            [](const auto& value) {
                return Serde<std::decay_t<decltype(value)>>::serialize(value);
            },
            var));
        return buffer;
    }

    template <Reader Invocable>
    static std::variant<Args...> deserialize(Invocable&& reader) {
        size_t index = Serde<size_t>::deserialize(std::forward<Invocable>(reader));

        return [&]<size_t... Is>(std::index_sequence<Is...>) {
            using variant_type = std::variant<Args...>;
            using deserialize_fn = variant_type (*)(Invocable&&);
            constexpr deserialize_fn fns[] = {[](Invocable&& reader) -> variant_type {
                using T = std::tuple_element_t<Is, std::tuple<Args...>>;
                return Serde<T>::deserialize(std::forward<Invocable>(reader));
            }...};
            if(index >= sizeof...(Args)) {
                throw std::runtime_error("Invalid variant index during deserialization");
            }
            return fns[index](std::forward<Invocable>(reader));
        }(std::make_index_sequence<sizeof...(Args)>{});
    }
};

}  // namespace catter::rpc::data
