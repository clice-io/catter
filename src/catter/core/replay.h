#pragma once

#include <filesystem>
#include <kota/async/runtime/task.h>

#include "handler.h"
#include "util/data.h"

namespace catter::core {

/// Plays the build a replay file recorded to the handler, as the inject mode
/// delivers a real one: each command to on_command, or to on_error when the
/// event has an error, and its result, when the event has one, to on_exit.
///
/// @return how the build ended: the file's `finish`, else exit 0
kota::task<data::process_result> replay_build(const std::filesystem::path& path, Handler& handler);

}  // namespace catter::core
