#pragma once
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <kota/deco/deco.h>

#include "runtime_driver.h"
#include "js/capi/type.h"

namespace catter::core {
namespace config {

struct RunMode {
    const RuntimeDriver* driver = default_runtime_driver();

    auto into(std::string_view text, const kota::deco::decl::IntoContext& context)
        -> std::optional<std::string>;
};

inline auto RunMode::into(std::string_view text, const kota::deco::decl::IntoContext& context)
    -> std::optional<std::string> {

    auto* runtime_driver = find_runtime_driver(text);
    if(runtime_driver == nullptr) {
        return context.format_error(std::format("Unsupported mode: {}", text));
    }

    this->driver = runtime_driver;
    return std::nullopt;
}

inline auto default_working_directory() noexcept -> std::filesystem::path {
    try {
        return std::filesystem::current_path();
    } catch(const std::exception&) {
        return {};
    }
}

struct WorkingDirectory {
    std::filesystem::path path = default_working_directory();

    auto into(std::string_view text, const kota::deco::decl::IntoContext& context)
        -> std::optional<std::string> {
        try {
            path = std::filesystem::absolute(text);
        } catch(const std::exception& ex) {
            return context.format_error(std::format("Wrong Path: {}", ex.what()));
        }
        return std::nullopt;
    }
};

}  // namespace config

struct CatterConfig {

    constexpr inline static kota::deco::decl::Category catter_category = {
        .exclusive = true,
        .required = true,
        .name = "OPTIONS",
        .description = "options of catter",
    };

    DECO_CFG_START(category = catter_category)

    DecoInput(meta_var = "<Script>",
              help = "path to the custom js; or internal script, eg. script::cdb",
              required = true)
    <std::string> script_path;

    DecoKV(names = {"-m", "--mode"},
           meta_var = "<Mode>",
           help = "mode of operation, e.g. 'inject'",
           required = false)
    <config::RunMode> mode = config::RunMode{};

    DecoKV(names = {"-d", "--dir"},
           meta_var = "<Working Directory>",
           help = "working directory for the target program, default to current directory",
           required = false)
    <config::WorkingDirectory> working_dir = config::WorkingDirectory{};

    DecoKV(
        names = {"--stdio-mode"},
        meta_var = "<inherit|capture>",
        help =
            "control target stdio behavior when it is captured by catter; 'inherit' prints in real time, 'capture' only stores it; default to 'inherit'",
        required = false)
    <js::CatterOptions::StdioMode> stdio_mode = js::CatterOptions::StdioMode::inherit;

    DecoPack(
        meta_var = "<Args>",
        help =
            "the command you want catter to use, must be after a '--'; you can also pass it in your script",
        required = false)
    <std::vector<std::string>> command = std::vector<std::string>{};

    std::vector<std::string> script_args;

    DecoFlag(names = {"-h", "--help"}, help = "show this help message", required = false)
    help = false;

    DECO_CFG_END()

    bool log = true;
};

}  // namespace catter::core
