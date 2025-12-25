#include <cstdint>
#include <fstream>
#include <print>
#include <quickjs.h>
#include <string>
#include <system_error>
#include "../apitool.h"
#include "js.h"
#include "qjs.h"
#include <filesystem>

namespace fs = std::filesystem;
using namespace catter::capi::util;

namespace {
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
    return catter::core::js::get_global_runtime_config().pwd.string();
}

CAPI(fs_path_join_all, (catter::qjs::Object path_parts)->std::string) {
    fs::path result;
    auto len = path_parts["length"].to<uint32_t>().value();
    for(size_t i = 0; i < len; ++i) {
        auto part = path_parts[std::to_string(i)].to<std::string>().value();
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
}  // namespace
