module;

#include "modules/prelude.h"

module catter;

import :feature.path;

namespace catter::feature {

std::string absolute_path(std::string_view cwd, std::string_view file) {
    using kota::fs::path::Style;
    std::string path;
    if(!kota::fs::path::is_absolute(file, Style::Posix) &&
       !kota::fs::path::is_absolute(file, Style::Windows)) {
        path.assign(cwd);
    }
    kota::fs::path::push(path, file);
    kota::fs::path::normalize(path);
    return path;
}

}  // namespace catter::feature
