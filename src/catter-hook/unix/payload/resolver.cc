#include "resolver.h"

namespace catter {
std::expected<std::filesystem::path, int>
    Resolver::from_current_directory(std::string_view file) const {
    return hook::shared::resolve_path_like(file);
}

std::expected<std::filesystem::path, int> Resolver::from_path(std::string_view file,
                                                              const char** envp) const {
    return hook::shared::resolve_from_environment(file, envp);
}

std::expected<std::filesystem::path, int>
    Resolver::from_search_path(std::string_view file, const char* search_path) const {
    return hook::shared::resolve_from_search_path(file, search_path == nullptr ? "" : search_path);
}
}  // namespace catter
