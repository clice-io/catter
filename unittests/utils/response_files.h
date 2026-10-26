#pragma once
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <kota/support/buffer_ref.h>

namespace catter {

/// The files a test's commands name, by path: what they read in place of the disk's.
struct ResponseFiles {
    std::map<std::string, std::string, std::less<>> files;

    std::error_code operator() (kota::buffer_ref out, std::string_view path) const {
        auto file = files.find(path);
        if(file == files.end()) {
            return std::make_error_code(std::errc::no_such_file_or_directory);
        }
        out.assign(file->second);
        return {};
    }
};

}  // namespace catter
