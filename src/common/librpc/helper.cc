#include "helper.h"
#include <string>

namespace {
std::string quote_win32_arg(std::string_view arg) {
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
}  // namespace

namespace catter::rpc::helper {

std::string cmdlineOf(const catter::rpc::data::Command& cmd) {
#ifdef _WIN32
    std::string full_cmd = quote_win32_arg(cmd.executable);
    for(const auto& arg: cmd.args) {
        full_cmd += " " + detail::quote_win32_arg(arg);
    }
    return full_cmd;
#else
    std::string full_cmd = cmd.executable;
    for(const auto& arg: cmd.args) {
        full_cmd += " " + arg;
    }
    return full_cmd;
#endif
}
}  // namespace catter::rpc::helper
