#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ipc.h"
#include "./option.h"
#include "js/capi/type.h"

namespace catter::app {

std::string load_script_content(const std::string& script_path);

}  // namespace catter::app
