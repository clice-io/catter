#include "feature/path.h"

#include <filesystem>
#include <string>
#include <string_view>

#include "support/encoding.h"

namespace catter::feature {

namespace {

bool is_absolute(std::string_view path) {
    const auto drive = path.size() >= 3 &&
                       ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
                       path[1] == ':' && (path[2] == '/' || path[2] == '\\');
    return path.starts_with('/') || path.starts_with(R"(\\)") || drive;
}

}  // namespace

std::string absolute_path(std::string_view cwd, std::string_view file) {
    auto path = is_absolute(file) ? from_utf8(file) : from_utf8(cwd) / from_utf8(file);
    return to_utf8(path.lexically_normal());
}

}  // namespace catter::feature
