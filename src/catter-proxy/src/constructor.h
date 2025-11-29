#pragma once
#include "librpc/data.h"

namespace catter::proxy {
rpc::data::command build_raw_cmd(char* arg_start[], char* arg_end[]);
}  // namespace catter::proxy
