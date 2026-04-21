#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <kota/ipc/protocol.h>

namespace catter::data {

using ipcid_t = int32_t;
using timestamp_t = uint64_t;

struct command {
    std::string cwd{};
    std::string executable{};
    std::vector<std::string> args{};
    std::vector<std::string> env{};
};

struct process_result {
    int64_t code = -1;
    std::string std_out{};
    std::string std_err{};
};

struct action {
    enum : uint8_t {
        DROP,    // Do not execute the command
        INJECT,  // Inject <catter-payload> into the command
        WRAP,    // Wrap the command execution, and return its exit code
    } type;

    command cmd;
};

enum class ServiceMode : uint8_t {
    INJECT,
};

}  // namespace catter::data

namespace catter::ipc {

enum class RequestType : uint8_t {
    CHECK_MODE,
    CREATE,
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
};

template <RequestType Type>
struct Request;

template <>
struct Request<RequestType::CHECK_MODE> {
    using Params = data::ServiceMode;
    using Result = bool;
    constexpr inline static std::string_view method = "check_mode";
};

template <>
struct Request<RequestType::CREATE> {
    using Params = data::ipcid_t;
    using Result = data::ipcid_t;
    constexpr inline static std::string_view method = "create";
};

template <>
struct Request<RequestType::MAKE_DECISION> {
    using Params = data::command;
    using Result = data::action;
    constexpr inline static std::string_view method = "make_decision";
};

template <>
struct Request<RequestType::REPORT_ERROR> {
    struct Params {
        data::ipcid_t parent_id;
        std::string error_msg;
    };

    using Result = std::nullptr_t;
    constexpr inline static std::string_view method = "report_error";
};

template <>
struct Request<RequestType::FINISH> {
    using Params = data::process_result;
    using Result = std::nullptr_t;
    constexpr inline static std::string_view method = "finish";
};
};  // namespace catter::ipc

namespace kota::ipc::protocol {
using namespace catter::ipc;

template <RequestType Type>
struct RequestTraits<Request<Type>> : Request<Type> {};

}  // namespace kota::ipc::protocol
