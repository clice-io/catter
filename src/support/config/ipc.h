#pragma once
#include <format>
#include <string>
#include <string_view>
#include <kota/fs/path.h>
#include <kota/sys/process.h>

#include "support/paths.h"

namespace catter::config {

/// The pipe a catter session listens on, one for each session so that
/// sessions running at once never meet: a named pipe on Windows, a socket in
/// catter's data directory elsewhere. Its proxies take it on their command
/// line, `--ipc <pipe>`.
inline std::string session_pipe_name() {
#ifdef CATTER_WINDOWS
    return std::format(R"(\\.\pipe\catter-ipc-{})", kota::sys::pid());
#else
    return data_path(std::format("ipc-{}.sock", kota::sys::pid()));
#endif
}

/// The path of `name` in the data directory of the session listening on
/// `pipe`, where its socket is: the session's, though a build step changed
/// HOME or CATTER_HOME. On Windows, where the pipe is in no directory, this
/// process's.
inline std::string session_data_path(std::string_view pipe, std::string_view name) {
#ifdef CATTER_WINDOWS
    return data_path(name);
#else
    std::string path(kota::fs::path::parent(pipe));
    kota::fs::path::push(path, name);
    return path;
#endif
}

}  // namespace catter::config
