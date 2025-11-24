#pragma once
#include "librpc/data.h"
#include <string>

namespace catter::rpc::helper {
std::string cmdline_of(const catter::rpc::data::command& cmd) noexcept;
}
