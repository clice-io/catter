#pragma once
#include <vector>
#include <string>
#include "librpc/data.h"
namespace catter::util {
std::vector<std::string> get_environment() noexcept;
// Just deposit here, it's cross-platform work, we need a clearly place
void locate_exe(rpc::data::command& command);
}  // namespace catter::util
