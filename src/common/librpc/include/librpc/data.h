#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace catter::rpc::data {

using command_id_t = int32_t;
using thread_id_t = int32_t;
using timestamp_t = uint64_t;



struct command {
    /// do not ensure that this is a file path, this may be the name in PATH env
    std::string executable;
    std::vector<std::string> args;
    std::vector<std::string> env;
};

struct action {
    enum : uint8_t {
        DROP,   // Do not execute the command
        INJECT, // Inject <catter-payload> into the command
        WRAP,   // Wrap the command execution, and return its exit code
    } type;
    command cmd;
};

}  // namespace catter::rpc::data
