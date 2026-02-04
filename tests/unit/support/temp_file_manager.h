#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace catter {
namespace fs = std::filesystem;

struct TempFileManager {
    fs::path root;

    TempFileManager(fs::path path) : root(std::move(path)) {}

    void create(const fs::path& file, std::error_code& ec, std::string_view content = "") noexcept {
        auto full_path = root / file;
        auto parent = full_path.parent_path();
        fs::create_directories(parent, ec);
        if(ec)
            return;

        std::ofstream ofs(full_path, std::ios::app);

        if(!ofs) {
            ec = std::make_error_code(std::errc::io_error);
            return;
        }
        ofs << content;
        ofs.close();
        fs::permissions(full_path,
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add,
                        ec);
    }

    ~TempFileManager() {
        std::error_code ec;
        if(fs::exists(root, ec)) {
            fs::remove_all(root, ec);
        }
    }

    TempFileManager(const TempFileManager&) = delete;
    TempFileManager& operator= (const TempFileManager&) = delete;
};

}  // namespace catter
