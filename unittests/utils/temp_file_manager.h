#pragma once
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <kotatsu/fs/directory.h>
#include <kotatsu/fs/file.h>
#include <kotatsu/fs/path.h>
#include <kotatsu/fs/status.h>
#include <kotatsu/fs/temp.h>
#include <kotatsu/sys/dirs.h>

namespace catter {

/// A directory of files a test writes, made in the system's temporary directory, named `prefix`
/// and random characters, and removed with them when this goes.
struct TempFileManager {
    kotatsu::fs::TempDirectory root;

    explicit TempFileManager(std::string_view prefix) :
        root([&] {
            std::string temp;
            if(auto error = kotatsu::sys::temp_dir(temp)) {
                throw std::system_error(error, "the temporary directory");
            }
            auto created = kotatsu::fs::TempDirectory::create(temp, prefix);
            if(!created) {
                throw std::system_error(created.error(), "a directory in " + temp);
            }
            return std::move(*created);
        }()) {}

    /// The path of `file` under the root.
    std::string path(std::string_view file) const {
        std::string path(root.path());
        kotatsu::fs::path::push(path, file);
        return path;
    }

    /// Writes `content` to `file` under the root, which anyone may run.
    std::error_code create(std::string_view file, std::string_view content = "") const {
        const auto full_path = path(file);
        if(auto created = kotatsu::fs::create_directories(kotatsu::fs::path::parent(full_path));
           !created) {
            return created.error();
        }
        if(auto error = kotatsu::fs::write_file(full_path, content)) {
            return error;
        }
        return kotatsu::fs::set_permissions(full_path, 0755);
    }
};

}  // namespace catter
