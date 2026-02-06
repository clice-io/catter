#pragma once

#include <string>
#include <vector>

namespace catter::util {
inline std::vector<std::string> save_argv(char* const argv[]) {
    std::vector<std::string> result;
    for(int i = 0; argv[i] != nullptr; ++i) {
        result.emplace_back(argv[i]);
    }
    return result;
}
}  // namespace catter::util
