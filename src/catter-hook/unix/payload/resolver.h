#pragma once

#include <expected>
#include <filesystem>
#include <string_view>

#include "shared/resolver.h"

namespace catter {

struct Resolver {
    [[nodiscard]]
    virtual std::expected<std::filesystem::path, int>
        from_current_directory(std::string_view file) const;

    [[nodiscard]]
    virtual std::expected<std::filesystem::path, int> from_path(std::string_view file,
                                                                const char** envp) const;

    [[nodiscard]]
    virtual std::expected<std::filesystem::path, int>
        from_search_path(std::string_view file, const char* search_path) const;
};
}  // namespace catter
