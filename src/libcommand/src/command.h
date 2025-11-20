#pragma once
#include <span>
#include <string>
#include <vector>
#include <filesystem>
#include <map>

namespace catter {

struct Command {
    std::filesystem::path exe;
    std::vector<std::string> args;
    std::map<std::string, std::string> env;

    std::string cmdline() const;

    using cstr_view = const char* const;

    static Command create(std::span<cstr_view> args, cstr_view envp[]);
};

int spawn(Command cmd);

}  // namespace catter
