module;

#include "modules/prelude.h"

module catter;

import :capture.backend;
import :driver.option;
import :protocol.remote_handler;
import :session.handler;
import :session.process;
import :session.session;

namespace catter::driver {

namespace {

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

kotatsu::async::task<kotatsu::async::process::exit_status>
    async_run(std::unique_ptr<Handler> builtin, const SessionOptions& options) {
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
    kotatsu::async::process::exit_status status;
    std::exception_ptr error;
    try {
        for(const auto& [connect, text]: options.handlers) {
            if(!connect) {
                remotes.push_back(RemoteHandler::spawn(text, handler_options(text)));
                continue;
            }
            remotes.push_back(co_await RemoteHandler::connect(text,
                                                              *options.allow_remote,
                                                              handler_options(text)));
        }

        std::vector<Handler*> chain;
        if(builtin) {
            chain.push_back(builtin.get());
        }
        for(const auto& remote: remotes) {
            chain.push_back(remote.get());
        }
        HandlerChain handler(std::move(chain));
        status = co_await run_session(handler, backend, std::move(build));
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

int run_build(std::unique_ptr<Handler> builtin, const SessionOptions& options) {
    auto [status] = kotatsu::async::run(async_run(std::move(builtin), options));
    return exit_like(*status);
}

}  // namespace catter::driver
