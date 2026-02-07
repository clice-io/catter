#pragma once
#include "util/ipc-data.h"

namespace catter::proxy {
ipc::data::command build_raw_cmd(char* arg_start[], char* arg_end[]);
}  // namespace catter::proxy
