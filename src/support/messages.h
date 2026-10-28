#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The messages catter exchanges over its session's pipe with what reports
// the build's commands: catter-proxy, and on Windows the hook itself; and the
// variables a program of the build finds that pipe in.

/// What the environment of a program of the build tells of the session it
/// reports its commands to.
namespace catter::env {

/// The session's pipe.
constexpr char IPC[] = "__key_catter_ipc_v1";

/// The command the program runs, which those it starts are children of.
constexpr char COMMAND_ID[] = "__key_catter_command_id_v1";

/// The directory of the wrapper backend's shims, set while a build runs under
/// it.
constexpr char SHIMS[] = "__key_catter_shims_v1";

/// catter-proxy, which the hook starts in place of each program.
constexpr char PROXY[] = "__key_catter_proxy_path_v1";

/// Set for catter-proxy alone, which then runs the build command the ptrace
/// backend traces: the socket it waits on until catter traces it, and tells
/// why the build command did not start. It removes the variable first.
constexpr char PTRACE[] = "__key_catter_ptrace_v1";

/// All of them but PTRACE, which a session sets anew for the build it runs.
constexpr std::array<std::string_view, 4> VARIABLES = {IPC, COMMAND_ID, SHIMS, PROXY};

}  // namespace catter::env

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
        /// Do not run the command: it exits 0.
        DROP,
        RUN,
        /// Do not run the command: the session has ended with an error.
        FAIL,
    } type;

    command cmd;
};

}  // namespace catter::data

namespace catter::ipc {

/// Each client of the pipe reports one command: what it is to do first, or that
/// it cannot be captured; then how it ended.
enum class RequestType : std::uint8_t {
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
    STARTED,
};

template <RequestType Type>
struct Request;

/// What catter decides for the command, which `parent` started, none for the
/// build command: the id it gives the command, which those the command starts
/// name as their parent, and what to do.
template <>
struct Request<RequestType::MAKE_DECISION> {
    struct Params {
        std::optional<data::ipcid_t> parent;
        data::command command;
    };

    struct Result {
        data::ipcid_t id;
        data::action action;
    };

    constexpr inline static std::string_view method = "make_decision";
};

/// catter cannot capture a command `parent` started: `message` says why.
template <>
struct Request<RequestType::REPORT_ERROR> {
    struct Params {
        std::optional<data::ipcid_t> parent;
        std::string message;
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
