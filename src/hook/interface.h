#pragma once
#include <expected>
#include <string>
#include <system_error>
#include <span>
#include <vector>

namespace catter::hook {
int run(std::span<const char* const> command, std::error_code& ec);

std::expected<std::vector<std::string>, std::string> collect_all();
};  // namespace catter::hook
