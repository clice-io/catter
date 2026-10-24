#pragma once
#include <format>
#include <string>
#include <kota/async/io/system.h>

#include "util/crossplat.h"
#include "util/encoding.h"

namespace catter::config::ipc {

/// The pipe a catter session listens on, one for each session so that
/// sessions running at once never meet: a named pipe on Windows, a socket in
/// catter's data directory elsewhere. Its proxies take it on their command
/// line, `--ipc <pipe>`.
inline std::string session_pipe_name() {
#ifdef CATTER_WINDOWS
    return std::format(R"(\\.\pipe\catter-ipc-{})", kota::sys::pid());
#else
    return util::to_utf8(util::get_catter_data_path() /
                         std::format("ipc-{}.sock", kota::sys::pid()));
#endif
}

}  // namespace catter::config::ipc
