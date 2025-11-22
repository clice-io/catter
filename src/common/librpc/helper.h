#pragma once
#include "librpc/data.h"
#include <string>

namespace catter::rpc::helper {
std::string cmdlineOf(const catter::rpc::data::Command& cmd);
}
