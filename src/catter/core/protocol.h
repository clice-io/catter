#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <kota/meta/annotation.h>
#include <kota/codec/macro.h>

#include "util/data.h"

/// The messages catter and a handler exchange (docs/en/design/handler-protocol.md):
/// JSON-RPC 2.0, each message framed by a Content-Length header. Field names
/// are in lower camel case, as kota::ipc::JsonCodec writes them; the SDKs'
/// types are generated from the schema of these.
namespace catter::protocol {

/// The protocol's version, which `initialize` carries.
constexpr int version = 1;

/// Bytes that are not valid UTF-8, in base64.
struct Base64 {
    std::string base64;
};

/// Bytes from the system, such as an argument, a path or an environment
/// entry: a string when they are valid UTF-8, else their base64.
using OSString = std::variant<std::string, Base64>;

OSString to_os_string(std::string bytes);

std::vector<OSString> to_os_string(std::vector<std::string> bytes);

/// The bytes, or what is wrong with the base64.
std::expected<std::string, std::string> to_bytes(OSString text);

std::expected<std::vector<std::string>, std::string> to_bytes(std::vector<OSString> texts);

/// `bytes` as a string of the protocol, which is valid UTF-8: each byte that
/// is not becomes U+FFFD.
std::string to_text(std::string bytes);

/// The build a session runs.
struct Build {
    std::vector<OSString> command;
    OSString cwd;
};

/// Request `initialize`, catter's first message.
struct InitializeParams {
    int version;
    /// Tells the sessions apart that one handler over TCP serves.
    std::string session;
    /// How the commands are captured: `inject`, or `replay` of a recorded
    /// build.
    std::string mode;
    Build build;
};

/// The notifications a handler takes.
struct Events {
    /// `command`, for a handler that does not decide.
    bool command;
    bool error;
    bool exit;
};

struct InitializeResult {
    /// Every command comes as a request, answered with an action.
    bool decides;
    Events events;
    /// Commands carry their environment.
    bool env;
};

/// Request `command` to a handler that decides, else a notification.
struct Command {
    data::ipcid_t id;
    /// The command that started this one, absent for the build command.
    std::optional<data::ipcid_t> parent;
    OSString cwd;
    OSString executable;
    std::vector<OSString> args;
    std::optional<std::vector<OSString>> env;
};

struct Skip {};

struct Drop {};

struct Abort {
    std::string reason;
};

/// Runs another command in the command's place: absent fields keep the
/// command's own.
struct Modify {
    std::vector<OSString> args;
    std::optional<OSString> executable;
    std::optional<OSString> cwd;
    std::optional<std::vector<OSString>> env;
};

KOTATSU_ANNOTATION(action_annotation,
                   tag = "action",
                   tag_names = {"skip", "drop", "abort", "modify"});

/// The result of `command`.
using Action =
    kota::meta::annotate<action_annotation>::type<std::variant<Skip, Drop, Abort, Modify>>;

/// Notification `error`: catter could not capture a command.
struct CaptureError {
    data::ipcid_t id;
    std::optional<data::ipcid_t> parent;
    std::string message;
};

/// Notification `exit`: a command has run.
struct Exit {
    data::ipcid_t id;
    std::int64_t code;
    /// From the decision on the command to its exit, in microseconds.
    std::int64_t duration;
};

/// Request `finish`, catter's last message: how the build ended.
struct Finish {
    std::int64_t code;
};

}  // namespace catter::protocol
