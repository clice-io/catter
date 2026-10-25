#include "app_runner.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <cpptrace/exceptions.hpp>

#include "handler.h"
#include "option.h"
#include "remote_handler.h"
#include "runtime_driver.h"
#include "builtin/builtin.h"
#include "js/js.h"
#include "js/script_handler.h"
#include "util/encoding.h"
#include "util/output.h"

namespace catter::app {

namespace {

std::string load_script_content(const std::string& script_path) {
    std::ifstream ifs{util::from_utf8(script_path)};
    if(!ifs.good()) {
        throw cpptrace::runtime_error(std::format("Failed to open script file `{}`", script_path));
    }

    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

/// The handler that runs the script `config` names: a built-in, or a file of JavaScript, which
/// `runtime` runs.
kota::task<std::unique_ptr<core::Handler>> load_script(js::RuntimeScope& runtime,
                                                       const core::CatterConfig& config) {
    const auto& script_path = *config.script_path;
    if(script_path.starts_with("script::")) {
        auto handler = builtin::make(script_path, config.script_args);
        if(!handler) {
            throw cpptrace::runtime_error(std::format("Unknown builtin script `{}`", script_path));
        }
        co_return handler;
    }

    runtime.start({.pwd = config.working_dir->path});
    auto script_absolute_path =
        util::to_utf8(std::filesystem::absolute(util::from_utf8(script_path)).lexically_normal());
    co_await js::run_script(load_script_content(script_absolute_path), script_absolute_path);
    co_return std::make_unique<js::ScriptHandler>(script_path, config.script_args, config.log);
}

/// Runs the session of `handler`: the build it starts, if any, and then its end.
kota::task<> run(core::Handler& handler, const core::CatterConfig& config) {
    auto build = co_await handler.on_start({
        .command = config.command.value(),
        .cwd = util::to_utf8(config.working_dir->path),
        .stdio_mode = config.stdio_mode.value(),
    });
    if(build) {
        auto result = co_await config.mode->driver->execute(*build, handler);
        co_await handler.on_finish(std::move(result));
    }
}

struct Address {
    /// The addresses of the host, in the order to try them.
    std::vector<std::string> hosts;

    int port;
};

/// `host:port`, the host of an IPv6 address in brackets.
Address parse_address(std::string_view address, bool allow_remote) {
    auto colon = address.rfind(':');
    int port = 0;
    if(colon != std::string_view::npos) {
        auto digits = address.substr(colon + 1);
        auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), port);
        if(error != std::errc() || end != digits.data() + digits.size() || port <= 0 ||
           port > 65535) {
            port = 0;
        }
    }
    if(port == 0) {
        throw cpptrace::runtime_error(
            std::format("`{}` is not an address for --connect: give it as host:port", address));
    }

    auto host = address.substr(0, colon);
    if(host.starts_with('[') && host.ends_with(']')) {
        host = host.substr(1, host.size() - 2);
    }
    if(host == "localhost") {
        // A handler listening on localhost may have taken either.
        return {
            .hosts = {"127.0.0.1", "::1"},
            .port = port
        };
    }
    // catter resolves no names: the host is an IPv4 or IPv6 address.
    if(host.empty() || (host.find_first_not_of("0123456789.") != std::string_view::npos &&
                        host.find(':') == std::string_view::npos)) {
        throw cpptrace::runtime_error(std::format(
            "`{}` names no IP address: give --connect the handler's IP address and port",
            address));
    }
    const bool local = host == "::1" || host.starts_with("127.");
    if(!local) {
        if(!allow_remote) {
            throw cpptrace::runtime_error(std::format(
                "`{}` is not on this machine, and a handler can change and drop the build's commands: give --allow-remote to connect to it",
                address));
        }
        output::yellowln("Warning: the handler at `{}` on another machine decides on the build",
                         address);
    }
    return {.hosts = {std::string(host)}, .port = port};
}

/// Tells this session from the others a handler over TCP serves.
std::string session_id() {
    std::random_device device;
    std::uniform_int_distribution<std::uint64_t> random;
    return std::format("{:016x}", random(device));
}

}  // namespace

kota::task<> async_run(const core::CatterConfig& config) {
    const auto& script_path = config.script_path;
    if(!script_path.has_value() && config.handlers.empty()) {
        throw cpptrace::runtime_error(
            "catter needs a script, or a handler to give the build's commands to (--handler, --connect)");
    }

    const auto handler_options = [&, session = session_id()](std::string_view name) {
        return core::RemoteHandler::Options{
            .name = std::string(name),
            .session = session,
            .mode = std::string(config.mode->driver->name()),
            .timeout = std::chrono::seconds(*config.handler_timeout),
        };
    };

    js::RuntimeScope runtime;
    std::unique_ptr<core::Handler> script;
    std::vector<std::unique_ptr<core::RemoteHandler>> remotes;
    std::exception_ptr error;
    try {
        if(script_path.has_value()) {
            script = co_await load_script(runtime, config);
        }
        for(const auto& [connect, text]: config.handlers) {
            if(!connect) {
                remotes.push_back(core::RemoteHandler::spawn(text, handler_options(text)));
                continue;
            }
            auto [hosts, port] = parse_address(text, *config.allow_remote);
            remotes.push_back(co_await core::RemoteHandler::connect(std::move(hosts),
                                                                    port,
                                                                    handler_options(text)));
        }

        std::vector<core::Handler*> chain;
        if(script) {
            chain.push_back(script.get());
        }
        for(const auto& remote: remotes) {
            chain.push_back(remote.get());
        }
        core::HandlerChain handler(std::move(chain));
        co_await run(handler, config);
    } catch(...) {
        error = std::current_exception();
    }

    for(const auto& remote: remotes) {
        co_await remote->close();
    }
    co_await runtime.stop();

    if(error) {
        std::rethrow_exception(error);
    }
}

}  // namespace catter::app
