#include "command.h"

namespace catter {
namespace detail {
inline std::string quote_win32_arg(std::string_view arg) {
    // No quoting needed if it's empty or has no special characters.
    if(arg.empty() || arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }

    std::string quoted_arg;
    quoted_arg.push_back('"');

    for(auto it = arg.begin();; ++it) {
        int num_backslashes = 0;
        while(it != arg.end() && *it == '\\') {
            ++it;
            ++num_backslashes;
        }

        if(it == arg.end()) {
            // End of string; append backslashes and a closing quote.
            quoted_arg.append(num_backslashes * 2, '\\');
            break;
        }

        if(*it == '"') {
            // Escape all backslashes and the following double quote.
            quoted_arg.append(num_backslashes * 2 + 1, '\\');
            quoted_arg.push_back(*it);
        } else {
            // Backslashes aren't special here.
            quoted_arg.append(num_backslashes, '\\');
            quoted_arg.push_back(*it);
        }
    }
    quoted_arg.push_back('"');
    return quoted_arg;
}

}  // namespace detail

std::string Command::cmdline() const {
#ifdef _WIN32
    std::string full_cmd = detail::quote_win32_arg(exe.string());
    for(const auto& arg: args) {
        full_cmd += " " + detail::quote_win32_arg(arg);
    }
    return full_cmd;
#else
    std::string full_cmd = exe.string();
    for(const auto& arg: args) {
        full_cmd += " " + arg;
    }
    return full_cmd;
#endif
}

Command Command::create(std::span<char_view> args, char_view envp[]) {

    if(args.size() == 0) {
        throw std::invalid_argument("No arguments provided");
    }

    // TODO: resolve exe path
    std::filesystem::path exe_path = args[0];

    std::vector<std::string> arguments = {args.begin() + 1, args.end()};
    std::map<std::string, std::string> environment;
    for(char_view* it = envp; *it != nullptr; ++it) {
        std::string_view env_entry(*it);
        size_t pos = env_entry.find('=');
        if(pos != std::string_view::npos) {
            environment.emplace(std::string(env_entry.substr(0, pos)),
                                std::string(env_entry.substr(pos + 1)));
        } else {
            environment.emplace(std::string(env_entry), "");
        }
    }
    return Command{std::move(exe_path), std::move(arguments), std::move(environment)};
}

int spawn(Command cmd) {
    // For demonstration purposes, just use std::system to run the command
    return std::system(cmd.cmdline().c_str());
}
}  // namespace catter
