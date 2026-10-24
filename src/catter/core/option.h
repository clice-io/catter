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

#include "handler.h"
#include "runtime_driver.h"
#include "util/encoding.h"

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
            path = std::filesystem::absolute(util::from_utf8(text));
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
              required = false)
    <std::string> script_path;

    DecoKV(
        names = {"--handler"},
        meta_var = "<Command>",
        help =
            "a handler for catter to start, which decides or observes the build's commands over its stdin and stdout; may be given more than once",
        required = false)
    <std::string> handler;

    DecoKV(
        names = {"--connect"},
        meta_var = "<Host:Port>",
        help =
            "a handler listening at the address, on this machine unless --allow-remote is given; may be given more than once",
        required = false)
    <std::string> connect;

    DecoFlag(names = {"--allow-remote"},
             help = "let --connect reach a handler on another machine",
             required = false)
    allow_remote = false;

    DecoKV(
        names = {"--handler-timeout"},
        meta_var = "<Seconds>",
        help =
            "how long catter waits for a handler to answer, and to exit once the session is over; 0 waits as long as it takes; default to 60",
        required = false)
    <unsigned> handler_timeout = 60;

    DecoKV(
        names = {"-m", "--mode"},
        meta_var = "<Mode>",
        help =
            "mode of operation: 'inject' (the default) runs the build; 'replay' plays a recorded one, whose replay file is the build command",
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
    <StdioMode> stdio_mode = StdioMode::inherit;

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

    /// A handler the command line names: a command for catter to start
    /// (--handler), or an address to connect to (--connect).
    struct Handler {
        bool connect;

        std::string text;
    };

    /// The handlers, in the order of the command line: an option given
    /// more than once keeps only its last value, so main collects them.
    std::vector<Handler> handlers;
};

}  // namespace catter::core
