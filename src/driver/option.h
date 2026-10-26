#pragma once
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <kota/deco/deco.h>
#include <kota/fs/resolve.h>

namespace catter::driver {

struct WorkingDirectory {
    std::string path = [] {
        std::string cwd;
        kota::fs::current_dir(cwd);
        return cwd;
    }();

    auto into(std::string_view text, const kota::deco::decl::IntoContext& context)
        -> std::optional<std::string> {
        if(auto error = kota::fs::absolute(path, text)) {
            return context.format_error(std::format("Wrong Path: {}", error.message()));
        }
        return std::nullopt;
    }
};

/// `-C`: the directory the command runs in.
struct DirectoryOption {
    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-C", "--directory", "--directory="},
                 meta_var = "<path>",
                 help = "The directory the command runs in. [default: the current directory]")
    <WorkingDirectory> directory = WorkingDirectory{};

    DECO_CFG_END()
};

/// What a subcommand that runs a build takes besides its own options: the build, where it
/// runs, and the handlers that decide on its commands.
struct SessionOptions {
    DirectoryOption directory;

    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"--from", "--from="},
                 meta_var = "<file>",
                 help = "Replay the build recorded in this file instead of running one.")
    <std::string> from;

    DecoKV(
        names = {"--handler"},
        meta_var = "<command>",
        help =
            "Start a handler, which decides or observes the build's commands over its stdin and stdout; may be given more than once.")
    <std::string> handler;

    DecoKV(
        names = {"--connect"},
        meta_var = "<host:port>",
        help =
            "Connect to a handler listening at the address, on this machine unless --allow-remote is given; may be given more than once.")
    <std::string> connect;

    DecoFlag(names = {"--allow-remote"}, help = "Let --connect reach a handler on another machine.")
    allow_remote = false;

    DecoKV(
        names = {"--handler-timeout"},
        meta_var = "<seconds>",
        help =
            "How long catter waits for a handler to answer, and to exit once the session is over; 0 waits as long as it takes. [default: 60]")
    <unsigned> handler_timeout = 60;

    DecoPack(meta_var = "<build command>", help = "The build to run.")
    <std::vector<std::string>> command = std::vector<std::string>{};

    DECO_CFG_END()

    /// A handler the command line names: a command for catter to start
    /// (--handler), or an address to connect to (--connect).
    struct HandlerOption {
        bool connect;

        std::string text;
    };

    /// The handlers, in the order of the command line: an option given
    /// more than once keeps only its last value, so the parse collects them.
    std::vector<HandlerOption> handlers;
};

}  // namespace catter::driver
