#pragma once

namespace catter::config {

/// What the environment of a program of the build tells of the session it
/// reports its commands to: the session's pipe, and the command the program
/// runs, which is the parent of those it starts.
constexpr static char KEY_CATTER_IPC[] = "__key_catter_ipc_v1";
constexpr static char KEY_CATTER_COMMAND_ID[] = "__key_catter_command_id_v1";

/// The directory of the wrapper backend's shims, set while a build runs under
/// it.
constexpr static char KEY_CATTER_SHIMS[] = "__key_catter_shims_v1";

}  // namespace catter::config
