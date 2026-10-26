module;

#include "modules/prelude.h"

module catter:capture.inject.hook;

import :session.data;

namespace catter {
/// The options that start the command with the hook, which reports the
/// commands it starts to the session's pipe as children of command `id`.
kota::process::options hook_options(data::command command,
                                    data::ipcid_t id,
                                    std::string_view pipe_name);
}  // namespace catter
