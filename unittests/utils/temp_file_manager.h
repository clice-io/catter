#pragma once
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <kota/fs/directory.h>
#include <kota/fs/file.h>
#include <kota/fs/path.h>
#include <kota/fs/status.h>
#include <kota/fs/temp.h>
#include <kota/sys/dirs.h>

namespace catter {

/// A directory of files a test writes, made in the system's temporary directory, named `prefix`
/// and random characters, and removed with them when this goes.
struct TempFileManager {
    kota::fs::TempDirectory root;

    explicit TempFileManager(std::string_view prefix) :
        root([&] {
            std::string temp;
            if(auto error = kota::sys::temp_dir(temp)) {
                throw std::system_error(error, "the temporary directory");
            }
            auto created = kota::fs::TempDirectory::create(temp, prefix);
            if(!created) {
                throw std::system_error(created.error(), "a directory in " + temp);
            }
            return std::move(*created);
        }()) {}

    /// The path of `file` under the root.
    std::string path(std::string_view file) const {
        std::string path(root.path());
        kota::fs::path::push(path, file);
        return path;
    }

    /// Writes `content` to `file` under the root, which anyone may run.
    std::error_code create(std::string_view file, std::string_view content = "") const {
        const auto full_path = path(file);
        if(auto created = kota::fs::create_directories(kota::fs::path::parent(full_path));
           !created) {
            return created.error();
        }
        if(auto error = kota::fs::write_file(full_path, content)) {
            return error;
        }
        return kota::fs::set_permissions(full_path, 0755);
    }
};

}  // namespace catter
