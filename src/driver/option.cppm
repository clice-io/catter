module;

#include "support/prelude.h"

module catter:driver.option;

import :session.handler;

namespace catter::driver {

struct WorkingDirectory {
    std::string path = [] {
        std::string cwd;
        fs::current_dir(cwd);
        return cwd;
    }();
};

/// `-C`: the directory the command runs in.
struct DirectoryOption {
    KANNOTATE(&directory,
              cli::names = {"-C", "--directory"},
              cli::meta_var = "<path>",
              description = "The directory the command runs in. [default: the current directory]")
    WorkingDirectory directory;
};

#ifdef CATTER_MAC
constexpr std::string_view capture_help =
    "How the build's commands are captured: hook, wrapper or eslogger. [default: hook]";
#elifdef CATTER_LINUX
constexpr std::string_view capture_help =
    "How the build's commands are captured: hook, wrapper or ptrace. [default: hook]";
#else
constexpr std::string_view capture_help =
    "How the build's commands are captured: hook or wrapper. [default: hook]";
#endif

/// What a subcommand that runs a build takes besides its own options: the build, where it
/// runs, and the handlers that decide on its commands.
struct SessionOptions {
    DirectoryOption directory;

    KANNOTATE(&capture, cli::meta_var = "<backend>", description = capture_help)
    std::optional<std::string> capture;

    KANNOTATE(&from,
              cli::meta_var = "<file>",
              description = "Replay the build recorded in this file instead of running one.")
    std::optional<std::string> from;

    KANNOTATE(
        &handler,
        cli::meta_var = "<command>",
        description =
            "Start a handler, which decides or observes the build's commands over its stdin and stdout; may be given more than once.")
    std::string handler;

    KANNOTATE(
        &connect,
        cli::meta_var = "<host:port>",
        description =
            "Connect to a handler listening at the address, on this machine unless --allow-remote is given; may be given more than once.")
    std::string connect;

    KANNOTATE(&allow_remote, description = "Let --connect reach a handler on another machine.")
    bool allow_remote = false;

    KANNOTATE(
        &handler_timeout,
        cli::meta_var = "<seconds>",
        description =
            "How long catter waits for a handler to answer, and to exit once the session is over; 0 waits as long as it takes. [default: 60]")
    unsigned handler_timeout = 60;

    KANNOTATE(&command,
              cli::rest = true,
              cli::meta_var = "<build command>",
              description = "The build to run.")
    std::vector<std::string> command;

    /// A handler the command line names: a command for catter to start
    /// (--handler), or an address to connect to (--connect).
    struct HandlerOption {
        bool connect;

        std::string text;
    };

    /// The handlers, in the order of the command line: an option given
    /// more than once keeps only its last value, so the parse collects them.
    KANNOTATE(&handlers, skip = true)
    std::vector<HandlerOption> handlers;
};

/// Runs the build `options` name, its commands decided by `builtin`, if any,
/// then by the handlers they name: the exit code catter ends with, as the build
/// ended; 0 when the handlers start none.
int run_build(std::unique_ptr<Handler> builtin, const SessionOptions& options);

}  // namespace catter::driver

template <>
struct kotatsu::codec::repr<catter::driver::WorkingDirectory> {
    using type = std::string;

    template <typename F>
    static bool decode(auto& reader, catter::driver::WorkingDirectory& directory) {
        std::string text;
        if(!reader.visit_str(text)) {
            return false;
        }
        if(auto error = kotatsu::fs::absolute(directory.path, text)) {
            return kotatsu::codec::scoped_context<kotatsu::codec::rich_error>::fail(
                kotatsu::codec::rich_error(std::format("Wrong Path: {}", error.message())));
        }
        return true;
    }

    template <typename F>
    static bool encode(auto& writer, const catter::driver::WorkingDirectory& directory) {
        return writer.visit_str(directory.path);
    }
};
