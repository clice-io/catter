#pragma once
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <kota/fs/directory.h>
#include <kota/fs/file.h>
#include <kota/fs/path.h>
#include <kota/fs/status.h>

namespace catter {

/// A directory of files a test writes, removed with them when this goes.
struct TempFileManager {
    std::string root;

    explicit TempFileManager(std::string path) : root(std::move(path)) {}

    /// The path of `file` under the root.
    std::string path(std::string_view file) const {
        auto path = root;
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

    ~TempFileManager() {
        kota::fs::remove_all(root);
    }

    TempFileManager(const TempFileManager&) = delete;
    TempFileManager& operator= (const TempFileManager&) = delete;
};

}  // namespace catter
