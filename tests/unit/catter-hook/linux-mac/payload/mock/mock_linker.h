#pragma once
#include <vector>
#include <string>
#include <expected>
#include "linker.h"

namespace ct = catter;

struct MockLinker : public ct::Linker {
    mutable std::string last_path;
    mutable std::vector<std::string> last_argv;
    mutable std::vector<std::string> last_envp;

    int return_value = 0;
    bool should_fail = false;
    int error_code = ENOENT;

    void reset() {
        last_path.clear();
        last_argv.clear();
        last_envp.clear();
        return_value = 0;
        should_fail = false;
        error_code = ENOENT;
    }

    std::expected<int, const char*> execve(const char* path,
                                           char* const argv[],
                                           char* const envp[]) const noexcept override {
        if(should_fail)
            return std::unexpected("mock linker error");

        last_path = path ? path : "";
        last_argv.clear();
        for(int i = 0; argv && argv[i]; ++i)
            last_argv.emplace_back(argv[i]);

        last_envp.clear();
        for(int i = 0; envp && envp[i]; ++i)
            last_envp.emplace_back(envp[i]);

        return return_value;
    }

    std::expected<int, const char*> posix_spawn(pid_t* pid,
                                                const char* path,
                                                const posix_spawn_file_actions_t* /*actions*/,
                                                const posix_spawnattr_t* /*attrp*/,
                                                char* const argv[],
                                                char* const envp[]) const noexcept override {
        return execve(path, argv, envp);
    }
};
