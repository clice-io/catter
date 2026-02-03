#pragma once
#include "session.h"
#include <climits>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace catter {
class CmdBuilder {
public:
    using ArgvRef = std::span<char* const>;

    struct command {
        using ArgvTy = std::vector<std::string>;
        std::string path;
        ArgvTy argv;

        bool valid() const noexcept {
            return !path.empty();
        }

        std::vector<const char*> c_argv() const {
            std::vector<const char*> res;
            res.reserve(argv.size() + 1);
            for(const auto& arg: argv) {
                res.push_back(arg.c_str());
            }
            res.push_back(nullptr);
            return res;
        }
    };

public:
    CmdBuilder(Session session) noexcept : session_(session) {};
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
    command proxy_cmd(const std::filesystem::path& path, ArgvRef argv) noexcept;
    /**
     * Build the error command string.
     * @param msg error message.
     * @param path of the executable.
     * @param argv of the executable.
     * @return the error command string.
     * @example linux or mac error found in hook: msg in executing:
     *              --> path arg1 arg2 ...
     */
    command error_cmd(const char* msg,
                      const std::filesystem::path& path,
                      ArgvRef argv = {}) noexcept;

private:
    Session session_;
};

}  // namespace catter
