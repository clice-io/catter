#pragma once

#include <string_view>

namespace catter::config::data {
constexpr std::string_view js_lib =
#include "js-lib.inc"
    ;
}  // namespace catter::config::data
