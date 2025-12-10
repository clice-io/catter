#include <cstdint>
#include <cstdlib>
#include <print>

#include <type_traits>
#include <vector>

#include "librpc/function.h"
#include "librpc/data.h"

// TODO
namespace catter::rpc::server {

#ifdef _WIN32
constexpr char PIPE_NAME[] = R"(\\.\pipe\114514)";
#endif

void write(std::vector<char>& payload) {}

void read(char* dst, size_t len) {}

data::decision_info make_decision(data::command_id_t parent_id, data::command cmd) {
    std::vector<char> data =
        data::merge_range_to_vector(
            data::Serde<data::Request>::serialize(data::Request::MAKE_DECISION),
            data::Serde<data::command>::serialize(cmd),
            data::Serde<data::command_id_t>::serialize(parent_id)
        );
    write(data);
    return data::Serde<data::decision_info>::deserialize(read);
}

void report_error(data::command_id_t parent_id, std::string error_msg) noexcept {
    try {
        std::vector<char> data =
            data::merge_range_to_vector(
                data::Serde<data::Request>::serialize(data::Request::REPORT_ERROR),
                data::Serde<data::command_id_t>::serialize(parent_id),
                data::Serde<std::string>::serialize(error_msg)
            );
        write(data);
    } catch(...) {
        // cannot do anything here
    }
    return;
};

void finish(int ret_code) {
    std::vector<char> data =
        data::merge_range_to_vector(
            data::Serde<data::Request>::serialize(data::Request::FINISH),
            data::Serde<int>::serialize(ret_code)
        );
    write(data);
    return;
}
}  // namespace catter::rpc::server
