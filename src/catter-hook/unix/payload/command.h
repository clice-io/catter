#pragma once
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "session.h"

namespace catter {

using ArgvRef = std::span<const char* const>;

struct Command {
    std::string path;
    std::vector<std::string> argv;

    [[nodiscard]]
    std::vector<char*> c_argv();
};

/**
 * Build the proxy command.
 *
 * @example /proxy_path -p self_id --exec /bin/cc -- cc -c main.cc
 */
[[nodiscard]]
Command build_proxy_command(const Session& session,
                            const std::filesystem::path& executable,
                            ArgvRef argv);

/**
 * Build the proxy error command.
 *
 * @example /proxy_path -p self_id --exec /bin/cc "Catter Proxy Error: ..."
 */
[[nodiscard]]
Command build_error_command(const Session& session,
                            std::string_view message,
                            const std::filesystem::path& executable,
                            ArgvRef argv = {});
}  // namespace catter
