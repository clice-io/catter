module;

#include "modules/prelude.h"

#include "support/output.h"

module catter;

import :capture.backend;
import :driver.app_runner;
import :protocol.remote_handler;
import :session.handler;
import :session.process;
import :session.session;

namespace catter::driver {

namespace {

/// Runs the session of `handler`: the build it starts, if any, and then its end.
kota::task<kota::process::exit_status> run(Handler& handler, const Backend& backend, Build build) {
    auto started = co_await handler.on_start(std::move(build));
    if(!started) {
        co_return kota::process::exit_status{};
    }
    Session session(handler);
    const auto start = std::chrono::steady_clock::now();
    auto status = co_await backend.run(*started, session);
    session.rethrow();
    co_await handler.on_finish(result_of(status, start));
    co_return status;
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
        output::warn("the handler at `{}` on another machine decides on the build", address);
    }
    return {.hosts = {std::string(host)}, .port = port};
}

/// The backend that captures the build: replay for a recorded one, else the one
/// --capture names.
const Backend& backend_of(const SessionOptions& options) {
    if(options.from.has_value()) {
        if(options.capture.has_value()) {
            throw cpptrace::runtime_error(
                "--from replays a recorded build, which nothing captures: give --capture with a build command after --");
        }
        return replay_backend();
    }
    const auto backends = capture_backends();
    if(!options.capture.has_value()) {
        return *backends.front();
    }
    if(auto found = std::ranges::find(backends, *options.capture, &Backend::name);
       found != backends.end()) {
        return **found;
    }
    throw cpptrace::runtime_error(
        std::format("`{}` is not a capture backend: give {}",
                    *options.capture,
                    std::ranges::to<std::string>(backends | std::views::transform(&Backend::name) |
                                                 std::views::join_with(std::string_view(" or ")))));
}

/// Tells this session from the others a handler over TCP serves.
std::string session_id() {
    std::random_device device;
    std::uniform_int_distribution<std::uint64_t> random;
    return std::format("{:016x}", random(device));
}

kota::task<kota::process::exit_status> async_run(std::unique_ptr<Handler> builtin,
                                                 const SessionOptions& options) {
    if(options.from.has_value() && !options.command->empty()) {
        throw cpptrace::runtime_error(
            "--from replays a recorded build in place of the build command after --: give one of them");
    }
    if(!options.from.has_value() && options.command->empty()) {
        throw cpptrace::runtime_error(
            "give the build command after --, or a recorded build with --from");
    }
    const auto& backend = backend_of(options);
    Build build{
        .command = options.from.has_value() ? std::vector{*options.from} : *options.command,
        .cwd = options.directory.directory->path,
    };

    const auto handler_options = [&, session = session_id()](std::string_view name) {
        return RemoteHandler::Options{
            .name = std::string(name),
            .session = session,
            .mode = std::string(backend.name()),
            .decisions = backend.follows_decisions(),
            .timeout = std::chrono::seconds(*options.handler_timeout),
        };
    };

    std::vector<std::unique_ptr<RemoteHandler>> remotes;
    kota::process::exit_status status;
    std::exception_ptr error;
    try {
        for(const auto& [connect, text]: options.handlers) {
            if(!connect) {
                remotes.push_back(RemoteHandler::spawn(text, handler_options(text)));
                continue;
            }
            auto [hosts, port] = parse_address(text, *options.allow_remote);
            remotes.push_back(
                co_await RemoteHandler::connect(std::move(hosts), port, handler_options(text)));
        }

        std::vector<Handler*> chain;
        if(builtin) {
            chain.push_back(builtin.get());
        }
        for(const auto& remote: remotes) {
            chain.push_back(remote.get());
        }
        HandlerChain handler(std::move(chain));
        status = co_await run(handler, backend, std::move(build));
    } catch(...) {
        error = std::current_exception();
    }

    for(const auto& remote: remotes) {
        co_await remote->close();
    }

    if(error) {
        std::rethrow_exception(error);
    }
    co_return status;
}

}  // namespace

kota::process::exit_status run_session(std::unique_ptr<Handler> builtin,
                                       const SessionOptions& options) {
    auto [status] = kota::run(async_run(std::move(builtin), options));
    return *status;
}

}  // namespace catter::driver
