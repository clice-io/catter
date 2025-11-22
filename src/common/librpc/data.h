#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace catter::rpc::data {

using command_id_t = int32_t;
using thread_id_t = int32_t;
using timestamp_t = uint64_t;

enum class Action : uint8_t {
    DROP,
    INJECT,
    NEGLECT,
};

struct Command {
    /// do not ensure that this is a file path, this may be the name in PATH env
    std::string executable;
    std::vector<std::string> args;
    std::vector<std::string> env;
};

struct DataToCatter {
    /// ensure when sending this cmd, the excutable is a file path
    Command cmd;
    command_id_t from_id;
    timestamp_t timestamp;
};

struct DataToProxy {
    /// Only support one command.
    /// more should use xx&&xx return it
    Command command;
    Action action;
    command_id_t new_cmd_id;
};

}  // namespace catter::rpc::data
