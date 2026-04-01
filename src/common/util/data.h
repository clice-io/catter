#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "util/serde.h"

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

enum class Request : uint8_t {
    CREATE,
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
};

template <Request Req>
struct RequestHelper {
    using RequestType = void;
};

template <>
struct RequestHelper<Request::CREATE> {
    using RequestType = ipcid_t(ipcid_t parent_id);
};

template <>
struct RequestHelper<Request::MAKE_DECISION> {
    using RequestType = action(command cmd);
};

template <>
struct RequestHelper<Request::REPORT_ERROR> {
    using RequestType = void(ipcid_t parent_id, std::string error_msg);
};

template <>
struct RequestHelper<Request::FINISH> {
    using RequestType = void(process_result result);
};

template <Request Req>
using RequestType = typename RequestHelper<Req>::RequestType;

using packet = std::vector<char>;

}  // namespace catter::data
