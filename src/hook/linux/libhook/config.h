#pragma once

namespace catter::config {
constexpr const inline static char KEY_CMD_LOG_FILE[] = "__catter_file_to_append_v1";
constexpr const inline static char KEY_PRELOAD[] = "LD_PRELOAD";
constexpr const inline static char LD_PRELOAD_INIT_ENTRY[] = "LD_PRELOAD=";
constexpr const inline static char KEY_CATTER_PRELOAD_PATH[] = "key_catter_preload_path_v1";
constexpr const inline static char ERROR_PREFFIX[] = "__catter_v1_error:";
constexpr const inline static char OS_PATH_SEPARATOR = ':';
constexpr const inline static char OS_DIR_SEPARATOR = '/';
}  // namespace catter::config
