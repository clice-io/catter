#include "driver/app_runner.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <cpptrace/exceptions.hpp>

#include "capture/runtime_driver.h"
#include "driver/option.h"
#include "feature/builtin.h"
#include "protocol/remote_handler.h"
#include "session/handler.h"
#include "support/encoding.h"
#include "support/output.h"

namespace catter::driver {

namespace {

/// Runs the session of `handler`: the build it starts, if any, and then its end.
kota::task<> run(Handler& handler, const CatterConfig& config) {
    auto build = co_await handler.on_start({
        .command = config.command.value(),
        .cwd = to_utf8(config.working_dir->path),
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

kota::task<> async_run(const CatterConfig& config) {
    if(!config.builtin.has_value() && config.handlers.empty()) {
        throw cpptrace::runtime_error(
            "catter needs a built-in, or a handler to give the build's commands to (--handler, --connect)");
    }

    const auto handler_options = [&, session = session_id()](std::string_view name) {
        return RemoteHandler::Options{
            .name = std::string(name),
            .session = session,
            .mode = std::string(config.mode->driver->name()),
            .timeout = std::chrono::seconds(*config.handler_timeout),
        };
    };

    std::unique_ptr<Handler> builtin_handler;
    if(config.builtin.has_value()) {
        builtin_handler = feature::make(*config.builtin, config.builtin_args);
        if(!builtin_handler) {
            throw cpptrace::runtime_error(std::format(
                "Unknown built-in `{}`: the built-ins are cdb, cmd-tree, target-tree and inspect",
                *config.builtin));
        }
    }

    std::vector<std::unique_ptr<RemoteHandler>> remotes;
    std::exception_ptr error;
    try {
        for(const auto& [connect, text]: config.handlers) {
            if(!connect) {
                remotes.push_back(RemoteHandler::spawn(text, handler_options(text)));
                continue;
            }
            auto [hosts, port] = parse_address(text, *config.allow_remote);
            remotes.push_back(
                co_await RemoteHandler::connect(std::move(hosts), port, handler_options(text)));
        }

        std::vector<Handler*> chain;
        if(builtin_handler) {
            chain.push_back(builtin_handler.get());
        }
        for(const auto& remote: remotes) {
            chain.push_back(remote.get());
        }
        HandlerChain handler(std::move(chain));
        co_await run(handler, config);
    } catch(...) {
        error = std::current_exception();
    }

    for(const auto& remote: remotes) {
        co_await remote->close();
    }

    if(error) {
        std::rethrow_exception(error);
    }
}

}  // namespace catter::driver
