#include "librpc/data.h"
#include <expected>

namespace catter::proxy {
std::expected<rpc::data::Command, std::string> buildRawCommand(char** arg_start, char** arg_end);
std::expected<rpc::data::DataToCatter, std::string> buildData(rpc::data::Command cmd,
                                                              rpc::data::command_id_t from_id);
}  // namespace catter::proxy
