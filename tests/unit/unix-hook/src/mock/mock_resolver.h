#pragma once
#include <boost/ut.hpp>
#include <string>
#include <expected>
#include <unordered_map>
#include "resolver.h"

namespace ct = catter;
namespace fs = std::filesystem;

struct MockResolver : public ct::Resolver {
    std::unordered_map<std::string, fs::path> mock_fs;

    void add_file(const std::string& name, const std::string& abs_path) {
        mock_fs[name] = abs_path;
    }

    std::expected<fs::path, int> from_current_directory(std::string_view file) const override {
        std::string key(file);
        if(mock_fs.contains(key)) {
            return mock_fs.at(key);
        }
        return std::unexpected(ENOENT);
    }

    std::expected<fs::path, int> from_path(std::string_view file,
                                           const char** /*envp*/) const override {
        std::string key(file);
        if(mock_fs.contains(key)) {
            return mock_fs.at(key);
        }
        return std::unexpected(ENOENT);
    }

    std::expected<fs::path, int> from_search_path(std::string_view file,
                                                  const char* /*search_path*/) const override {
        std::string key(file);
        if(mock_fs.contains(key)) {
            return mock_fs.at(key);
        }
        return std::unexpected(ENOENT);
    }
};
