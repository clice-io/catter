#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>
#include <uv.h>
#include <kota/async/io/fs.h>

#include "../apitool.h"
#include "../js.h"
#include "../qjs.h"

namespace fs = std::filesystem;
namespace qjs = catter::qjs;
using namespace catter::capi::util;

namespace {
constexpr int DEFAULT_DIR_MODE = 0777;
constexpr int DEFAULT_FILE_MODE = 0666;
constexpr int ACCESS_EXISTS = 0;
constexpr std::size_t FILE_CHUNK_SIZE = 64 * 1024;
constexpr std::uint64_t FILE_TYPE_MASK = 0170000;
constexpr std::uint64_t REGULAR_FILE_MODE = 0100000;
constexpr std::uint64_t DIRECTORY_MODE = 0040000;

template <typename T>
using JsTask = kota::task<T, std::string>;

using JsVoidTask = kota::task<void, std::string>;

bool is_missing(kota::error err) noexcept {
    return err == kota::error::no_such_file_or_directory || err == kota::error::not_a_directory;
}

bool is_regular_file(kota::fs::file_stats stats) noexcept {
    return (stats.mode & FILE_TYPE_MASK) == REGULAR_FILE_MODE;
}

bool is_directory(kota::fs::file_stats stats) noexcept {
    return (stats.mode & FILE_TYPE_MASK) == DIRECTORY_MODE;
}

std::string error_message(std::string_view action, std::string_view path, kota::error err) {
    return std::format("Failed to {} `{}`: {}", action, path, err.message());
}

kota::task<void, kota::error> create_directories_async(fs::path path) {
    fs::path current;
    for(const auto& part: path.lexically_normal()) {
        current /= part;
        if(current.empty()) {
            continue;
        }

        auto status = co_await kota::fs::stat(current.string());
        if(status) {
            if(!is_directory(*status)) {
                co_await kota::fail(kota::error::not_a_directory);
            }
            continue;
        }

        if(!is_missing(status.error())) {
            co_await kota::fail(status.error());
        }

        auto made = co_await kota::fs::mkdir(current.string(), DEFAULT_DIR_MODE);
        if(!made && made.error() != kota::error::file_already_exists) {
            co_await kota::fail(made.error());
        }
    }
    co_return;
}

kota::task<void, kota::error> remove_all_async(fs::path path) {
    auto status = co_await kota::fs::lstat(path.string());
    if(!status) {
        if(is_missing(status.error())) {
            co_return;
        }
        co_await kota::fail(status.error());
    }

    if(is_directory(*status)) {
        auto entries = co_await kota::fs::scandir(path.string());
        if(!entries) {
            co_await kota::fail(entries.error());
        }

        for(const auto& entry: *entries) {
            co_await remove_all_async(path / entry.name).or_fail();
        }

        co_await kota::fs::rmdir(path.string()).or_fail();
        co_return;
    }

    co_await kota::fs::unlink(path.string()).or_fail();
    co_return;
}

CAPI(fs_exists, (std::string path)->bool) {
    std::error_code ec;
    bool res = fs::exists(absolute_of(path), ec);
    if(ec) {
        throw catter::qjs::Exception("Failed to check existence of path: " + path +
                                     ", error: " + ec.message());
    }
    return res;
}

CAPI(fs_is_file, (std::string path)->bool) {
    std::error_code ec;
    auto res = fs::is_regular_file(absolute_of(path), ec);
    if(ec) {
        throw catter::qjs::Exception("Failed to check if path is file: " + path +
                                     ", error: " + ec.message());
    }
    return res;
}

CAPI(fs_is_dir, (std::string path)->bool) {
    std::error_code ec;
    auto res = fs::is_directory(absolute_of(path), ec);
    if(ec) {
        throw catter::qjs::Exception("Failed to check if path is directory: " + path +
                                     ", error: " + ec.message());
    }
    return res;
}

CAPI(fs_pwd, ()->std::string) {
    std::error_code ec;
    return catter::js::get_global_runtime_config().pwd.string();
}

CAPI(fs_path_join_all, (catter::qjs::Object path_parts)->std::string) {
    fs::path result;
    auto len = path_parts["length"].as<uint32_t>();
    for(size_t i = 0; i < len; ++i) {
        auto part = path_parts[std::to_string(i)].as<std::string>();
        if(i == 0) {
            result = fs::path(part);
        } else {
            result /= part;
        }
    }
    return result.string();
}

/// return root if no ancestor when doing this part
CAPI(fs_path_ancestor_n, (std::string path, uint32_t n)->std::string) {
    fs::path p = path;
    for(uint32_t i = 0; i < n; ++i) {
        p = p.parent_path();
    }
    return p.string();
}

/// use it when it is a file please
CAPI(fs_path_filename, (std::string path)->std::string) {
    fs::path p = path;
    return p.filename().string();
}

CAPI(fs_path_extension, (std::string path)->std::string) {
    fs::path p = path;
    return p.extension().string();
}

CAPI(fs_path_relative_to, (std::string base, std::string path)->std::string) {
    auto rel = fs::relative(absolute_of(path), absolute_of(base));
    return rel.string();
}

CAPI(fs_path_absolute, (std::string path)->std::string) {
    return absolute_of(path).string();
}

CAPI(fs_path_lexical_normal, (std::string path)->std::string) {
    fs::path p = path;
    return p.lexically_normal().string();
}

CAPI(fs_create_dir_recursively, (std::string path)->bool) {
    std::error_code ec;
    auto res = fs::create_directories(absolute_of(path), ec);
    if(ec) {
        throw catter::qjs::Exception("Failed to create directory: " + path +
                                     ", error: " + ec.message());
    }
    return res;
}

CAPI(fs_create_empty_file_recursively, (std::string path)->bool) {
    fs::path p = absolute_of(path);
    std::error_code ec;
    auto parent = p.parent_path();
    if(!fs::exists(parent, ec)) {
        fs::create_directories(parent, ec);
    }
    if(ec) {
        throw catter::qjs::Exception("Failed to create parent directories for file: " + path +
                                     ", error: " + ec.message());
    }
    std::ofstream ofs(p.string(), std::ios::app);
    if(!ofs.is_open()) {
        return false;
    }
    return true;
}

CAPI(fs_remove_recursively, (std::string path)->void) {
    std::error_code ec;
    fs::remove_all(absolute_of(path), ec);
    if(ec) {
        throw catter::qjs::Exception("Failed to remove path: " + path + ", error: " + ec.message());
    }
}

CAPI(fs_rename_if_exists, (std::string js_old_path, std::string js_new_path)->bool) {
    auto old_path = absolute_of(js_old_path);
    auto new_path = absolute_of(js_new_path);
    std::error_code ec;
    if(!fs::exists(old_path, ec)) {
        return false;
    }
    fs::rename(old_path, new_path, ec);
    if(ec) {
        throw catter::qjs::Exception("Failed to rename path from: " + js_old_path + " to " +
                                     js_new_path + ", error: " + ec.message());
    }
    return true;
}

CTX_CAPI(fs_list_dir, (JSContext * ctx, std::string path)->catter::qjs::Object) {
    fs::path p = absolute_of(path);
    std::error_code ec;
    if(!fs::is_directory(p, ec)) {
        throw catter::qjs::Exception("Path is not a directory: " + path);
    }
    auto res_arr = catter::qjs::Array<std::string>::empty_one(ctx);
    for(const auto& entry: fs::directory_iterator(p, ec)) {
        res_arr.push(entry.path().string());
    }
    if(ec) {
        throw catter::qjs::Exception("Failed to list directory: " + path +
                                     ", error: " + ec.message());
    }
    return catter::qjs::Object::from(std::move(res_arr));
}

ASYNC_CAPI(fs_async_exists, (std::string path)->JsTask<bool>) {
    auto abs_path = absolute_of(path).string();
    auto result = co_await kota::fs::access(abs_path, ACCESS_EXISTS);
    if(result) {
        co_return true;
    }

    if(is_missing(result.error())) {
        co_return false;
    }

    co_await kota::fail(error_message("check existence of", abs_path, result.error()));
}

ASYNC_CAPI(fs_async_is_file, (std::string path)->JsTask<bool>) {
    auto abs_path = absolute_of(path).string();
    auto result = co_await kota::fs::stat(abs_path);
    if(result) {
        co_return is_regular_file(*result);
    }

    if(is_missing(result.error())) {
        co_return false;
    }

    co_await kota::fail(error_message("stat", abs_path, result.error()));
}

ASYNC_CAPI(fs_async_is_dir, (std::string path)->JsTask<bool>) {
    auto abs_path = absolute_of(path).string();
    auto result = co_await kota::fs::stat(abs_path);
    if(result) {
        co_return is_directory(*result);
    }

    if(is_missing(result.error())) {
        co_return false;
    }

    co_await kota::fail(error_message("stat", abs_path, result.error()));
}

CTX_ASYNC_CAPI(fs_async_list_dir,
               (JSContext * ctx, std::string path)->JsTask<catter::qjs::Object>) {
    auto abs_path = absolute_of(path).string();
    auto entries = co_await kota::fs::scandir(abs_path);
    if(!entries) {
        co_await kota::fail(error_message("list directory", abs_path, entries.error()));
    }

    auto result = qjs::Array<std::string>::empty_one(ctx);
    for(const auto& entry: *entries) {
        result.push((fs::path(abs_path) / entry.name).string());
    }
    co_return qjs::Object::from(std::move(result));
}

ASYNC_CAPI(fs_async_create_dir_recursively, (std::string path)->JsTask<bool>) {
    auto abs_path = absolute_of(path).string();
    auto result = co_await create_directories_async(abs_path);
    if(!result) {
        co_await kota::fail(error_message("create directory", abs_path, result.error()));
    }

    co_return true;
}

ASYNC_CAPI(fs_async_create_empty_file_recursively, (std::string path)->JsTask<bool>) {
    auto abs_path = absolute_of(path).string();
    auto parent_result = co_await create_directories_async(fs::path(abs_path).parent_path());
    if(!parent_result) {
        co_await kota::fail(
            error_message("create parent directories for", abs_path, parent_result.error()));
    }

    auto opened =
        co_await kota::fs::open(abs_path, UV_FS_O_CREAT | UV_FS_O_WRONLY, DEFAULT_FILE_MODE);
    if(!opened) {
        co_await kota::fail(error_message("create file", abs_path, opened.error()));
    }

    auto closed = co_await kota::fs::close(*opened);
    if(!closed) {
        co_await kota::fail(error_message("close file", abs_path, closed.error()));
    }

    co_return true;
}

ASYNC_CAPI(fs_async_remove_recursively, (std::string path)->JsVoidTask) {
    auto abs_path = absolute_of(path).string();
    auto result = co_await remove_all_async(abs_path);
    if(!result) {
        co_await kota::fail(error_message("remove", abs_path, result.error()));
    }
    co_return;
}

ASYNC_CAPI(fs_async_rename_if_exists,
           (std::string js_old_path, std::string js_new_path)->JsTask<bool>) {
    auto old_path = absolute_of(js_old_path).string();
    auto new_path = absolute_of(js_new_path).string();
    auto exists = co_await kota::fs::access(old_path, ACCESS_EXISTS);
    if(!exists) {
        if(is_missing(exists.error())) {
            co_return false;
        }
        co_await kota::fail(error_message("check existence of", old_path, exists.error()));
    }

    auto result = co_await kota::fs::rename(old_path, new_path);
    if(!result) {
        co_await kota::fail(std::format("Failed to rename `{}` to `{}`: {}",
                                        old_path,
                                        new_path,
                                        result.error().message()));
    }

    co_return true;
}

ASYNC_CAPI(fs_async_read_text, (std::string path)->JsTask<std::string>) {
    auto abs_path = absolute_of(path).string();
    auto opened = co_await kota::fs::open(abs_path, UV_FS_O_RDONLY);
    if(!opened) {
        co_await kota::fail(error_message("open", abs_path, opened.error()));
    }

    int fd = *opened;
    std::string content;
    std::vector<char> buffer(FILE_CHUNK_SIZE);

    while(true) {
        auto read = co_await kota::fs::read(fd, std::span<char>(buffer));
        if(!read) {
            co_await kota::fs::close(fd);
            co_await kota::fail(error_message("read", abs_path, read.error()));
        }

        if(*read == 0) {
            break;
        }
        content.append(buffer.data(), *read);
    }

    auto closed = co_await kota::fs::close(fd);
    if(!closed) {
        co_await kota::fail(error_message("close", abs_path, closed.error()));
    }

    co_return content;
}

ASYNC_CAPI(fs_async_write_text, (std::string path, std::string content)->JsVoidTask) {
    auto abs_path = absolute_of(path).string();
    auto opened = co_await kota::fs::open(abs_path,
                                          UV_FS_O_CREAT | UV_FS_O_TRUNC | UV_FS_O_WRONLY,
                                          DEFAULT_FILE_MODE);
    if(!opened) {
        co_await kota::fail(error_message("open", abs_path, opened.error()));
    }

    int fd = *opened;
    std::size_t written = 0;
    while(written < content.size()) {
        auto remaining = std::span<const char>(content.data() + written, content.size() - written);
        auto result = co_await kota::fs::write(fd, remaining);
        if(!result) {
            co_await kota::fs::close(fd);
            co_await kota::fail(error_message("write", abs_path, result.error()));
        }
        if(*result == 0) {
            co_await kota::fs::close(fd);
            co_await kota::fail("Failed to write `" + abs_path + "`: wrote zero bytes");
        }
        written += *result;
    }

    auto closed = co_await kota::fs::close(fd);
    if(!closed) {
        co_await kota::fail(error_message("close", abs_path, closed.error()));
    }
    co_return;
}
}  // namespace
