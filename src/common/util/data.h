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
namespace req {
struct Create {
    using Params = data::ipcid_t;
    using Result = data::ipcid_t;
    constexpr inline static std::string_view method = "create";
};

struct MakeDecision {
    using Params = data::command;
    using Result = data::action;
    constexpr inline static std::string_view method = "make_decision";
};

struct ReportError {
    struct Params {
        data::ipcid_t parent_id;
        std::string error_msg;
    };

    using Result = std::nullptr_t;
    constexpr inline static std::string_view method = "report_error";
};

struct Finish {
    using Params = data::process_result;
    using Result = std::nullptr_t;
    constexpr inline static std::string_view method = "finish";
};
}  // namespace req
};  // namespace catter::ipc

namespace kota::ipc::protocol {
using namespace catter::ipc;

template <>
struct RequestTraits<req::Create> : req::Create {};

template <>
struct RequestTraits<req::MakeDecision> : req::MakeDecision {};

template <>
struct RequestTraits<req::ReportError> : req::ReportError {};

template <>
struct RequestTraits<req::Finish> : req::Finish {};
}  // namespace kota::ipc::protocol
