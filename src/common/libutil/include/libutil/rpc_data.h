#pragma once
#include <cstdint>

#include "libutil/serde.h"

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

enum class Request : uint8_t {
    CREATE,
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
};
}  // namespace catter::rpc::data

namespace catter {
template <>
struct Serde<rpc::data::Request> {
    static std::vector<char> serialize(const rpc::data::Request& req) {
        return Serde<uint8_t>::serialize(static_cast<uint8_t>(req));
    }

    template <Reader Invocable>
    static rpc::data::Request deserialize(Invocable&& reader) {
        uint8_t value = Serde<uint8_t>::deserialize(std::forward<Invocable>(reader));
        return static_cast<rpc::data::Request>(value);
    }

    template <CoReader Invocable>
    static coro::Lazy<rpc::data::Request> co_deserialize(Invocable&& reader) {
        uint8_t value = co_await Serde<uint8_t>::co_deserialize(std::forward<Invocable>(reader));
        co_return static_cast<rpc::data::Request>(value);
    }
};

template <>
struct Serde<rpc::data::command> {
    static std::vector<char> serialize(const rpc::data::command& cmd) {
        return merge_range_to_vector(Serde<std::string>::serialize(cmd.executable),
                                     Serde<std::vector<std::string>>::serialize(cmd.args),
                                     Serde<std::vector<std::string>>::serialize(cmd.env));
    }

    template <Reader Invocable>
    static rpc::data::command deserialize(Invocable&& reader) {
        rpc::data::command cmd;
        cmd.executable = Serde<std::string>::deserialize(std::forward<Invocable>(reader));
        cmd.args = Serde<std::vector<std::string>>::deserialize(std::forward<Invocable>(reader));
        cmd.env = Serde<std::vector<std::string>>::deserialize(std::forward<Invocable>(reader));
        return cmd;
    }

    template <CoReader Invocable>
    static coro::Lazy<rpc::data::command> co_deserialize(Invocable&& reader) {
        rpc::data::command cmd;
        cmd.executable =
            co_await Serde<std::string>::co_deserialize(std::forward<Invocable>(reader));
        cmd.args = co_await Serde<std::vector<std::string>>::co_deserialize(
            std::forward<Invocable>(reader));
        cmd.env = co_await Serde<std::vector<std::string>>::co_deserialize(
            std::forward<Invocable>(reader));
        co_return cmd;
    }
};

template <>
struct Serde<rpc::data::action> {
    static std::vector<char> serialize(const rpc::data::action& act) {
        return merge_range_to_vector(Serde<uint8_t>::serialize(static_cast<uint8_t>(act.type)),
                                     Serde<rpc::data::command>::serialize(act.cmd));
    }

    template <Reader Invocable>
    static rpc::data::action deserialize(Invocable&& reader) {
        using enum_type = decltype(rpc::data::action::type);
        return {
            static_cast<enum_type>(Serde<uint8_t>::deserialize(std::forward<Invocable>(reader))),
            Serde<rpc::data::command>::deserialize(std::forward<Invocable>(reader))};
    }

    template <CoReader Invocable>
    static coro::Lazy<rpc::data::action> co_deserialize(Invocable&& reader) {
        using enum_type = decltype(rpc::data::action::type);
        rpc::data::action act;
        act.type = static_cast<enum_type>(
            co_await Serde<uint8_t>::co_deserialize(std::forward<Invocable>(reader)));
        act.cmd =
            co_await Serde<rpc::data::command>::co_deserialize(std::forward<Invocable>(reader));
        co_return act;
    }
};
}  // namespace catter
