#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The messages catter exchanges over its session's pipe with what reports
// the build's commands: catter-proxy on Unix, the hook itself on Windows.

namespace catter::data {

using ipcid_t = std::int32_t;

struct command {
    std::string cwd{};
    std::string executable{};
    std::vector<std::string> args{};
    std::vector<std::string> env{};
};

struct process_result {
    std::int64_t code = -1;

    /// How long the program ran, in microseconds.
    std::int64_t duration = 0;
};

/// What catter decides for a command; `cmd` is the command to start, the one
/// it was sent unless a handler changed it.
struct action {
    enum : std::uint8_t {
        DROP,  // Do not execute the command
        RUN,   // Execute the command
        FAIL,  // Do not execute the command: the session has ended with an error
    } type;

    command cmd;
};

}  // namespace catter::data

namespace catter::ipc {

enum class RequestType : std::uint8_t {
    CREATE,
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
    STARTED,
};

template <RequestType Type>
struct Request;

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

/// The command's program runs as process `pid`. On Windows the hook asks it:
/// it started the program suspended, catter makes it run as decided, the hook
/// injected into it, and waits for its exit, and the result says whether it
/// could; the hook resumes the program then, or else ends it. On macOS
/// catter-proxy sends it as a notification, without waiting for an answer:
/// macOS has nothing to end a child with its parent, and catter ends the
/// program of a proxy that goes without its finish.
template <>
struct Request<RequestType::STARTED> {
    struct Params {
        std::int64_t pid;
    };

    using Result = bool;
    constexpr inline static std::string_view method = "started";
};

}  // namespace catter::ipc
