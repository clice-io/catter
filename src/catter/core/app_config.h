#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "capi/type.h"
#include "ipc.h"
#include "./option.h"

namespace catter::app {

std::string load_script_content(const std::string& script_path);

}  // namespace catter::app
