#pragma once
#include "buffer.h"
#include <climits>

namespace catter {
class CmdBuilder {
public:
    struct command {
        const char* path;
        char* const* argv;

        bool valid() const noexcept {
            return path != nullptr;
        }
    };

public:
    CmdBuilder(const char* proxy_path, const char* self_id) noexcept;
    CmdBuilder(const CmdBuilder&) = delete;
    CmdBuilder& operator= (const CmdBuilder&) = delete;
    CmdBuilder(CmdBuilder&&) noexcept = delete;
    CmdBuilder& operator= (CmdBuilder&&) noexcept = delete;
    ;

    /**
     * Build the proxy command string.
     * @param path of the executable.
     * @param argv of the executable.
     * @return the command string.
     * @example /proxy_path -p self_id -- path arg1 arg2 ...
     */
    command proxy_str(const char* path, char* const* argv) noexcept;
    /**
     * Build the error command string.
     * @param msg error message.
     * @param path of the executable.
     * @param argv of the executable.
     * @return the error command string.
     * @example linux or mac error found in hook: msg in executing:
     *              --> path arg1 arg2 ...
     */
    command error_str(const char* msg, const char* path, char* const* argv = nullptr) noexcept;

private:
    // Helper function to store a string as an argument
    const char* store_arg(Buffer& buf, const char* str) noexcept;

private:
    constexpr static auto BUF_SIZE = PATH_MAX * 100;
    constexpr static auto MAX_ARGC = PATH_MAX;
    constexpr static auto ARGV_RESERVED = 4;

    char cmd_buf_area[BUF_SIZE]{0};
    char* argv[MAX_ARGC]{nullptr};
    char* append_ptr;
    char** append_argv_ptr;
    const char* executable_path_ptr;
};

}  // namespace catter
