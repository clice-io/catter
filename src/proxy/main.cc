#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/async/async.h>
#include <kota/deco/deco.h>
#include <kota/fs/resolve.h>
#include <kota/sys/env.h>
#include <kota/sys/process.h>

#ifndef CATTER_WINDOWS
#include <pthread.h>
#include <sys/resource.h>
#endif

#include "capture/inject/hook.h"
#include "proxy/ipc.h"
#include "proxy/option.h"
#include "session/process.h"
#include "support/config/catter-proxy.h"
#include "support/guard.h"
#include "support/log.h"
#include "support/output.h"
#include "support/paths.h"

using namespace catter;

namespace {
using catter::data::action;
using exit_status = kota::process::exit_status;

kota::task<exit_status> run(data::action act,
                            data::ipcid_t id,
                            std::string pipe_name,
                            [[maybe_unused]] proxy::Peer& peer) {
    using catter::data::action;

    RunOptions stand_in{.stand_in = true};
#ifdef CATTER_MAC
    stand_in.started = [&](int pid) {
        peer.started(pid);
    };
#endif
    switch(act.type) {
        case action::WRAP: {
            auto opts = process_options(act.cmd);
            co_return co_await run_process(std::move(opts), std::move(stand_in));
        }
        case action::INJECT: {
            auto opts = hook_options(std::move(act.cmd), id, pipe_name);
            co_return co_await run_process(std::move(opts), std::move(stand_in));
        }
        case action::DROP: {
            co_return exit_status{.code = 0};
        }
        case action::FAIL: {
            co_return exit_status{.code = 1};
        }
        default: {
            co_return exit_status{.code = -1};
        }
    }
}

/// The command the proxy stands in for, as it runs here.
data::command command_of(const catter::proxy::ProxyOption& opt) {
    if(!opt.args.has_value()) {
        throw cpptrace::runtime_error("missing command arguments after --");
    }

    data::command cmd = {.args = *opt.args};
    if(auto error = kota::fs::current_dir(cmd.cwd)) {
        throw cpptrace::runtime_error(
            std::format("Failed to read the current directory: {}", error.message()));
    }
    for(const auto& [name, value]: kota::sys::environment()) {
        cmd.env.push_back(std::format("{}={}", name, value));
    }

    cmd.executable = opt.exec.has_value() ? *opt.exec : resolve_program(cmd.args.at(0));
    return cmd;
}

std::string format_args(std::span<const std::string> argv) {
    std::string result;
    for(const auto& arg: argv) {
        if(!result.empty()) {
            result += ' ';
        }
        result += arg;
    }
    return result;
}

/// What this process ends with: what its program ended with, the same signal
/// or the same exit code.
int exit_like(const exit_status& status) {
#ifndef CATTER_WINDOWS
    if(status.term_signal != 0) {
        // No core of this process beside the program's own.
        rlimit none{};
        ::setrlimit(RLIMIT_CORE, &none);
        std::signal(status.term_signal, SIG_DFL);
        // The build may have blocked it for the programs it starts.
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, status.term_signal);
        ::pthread_sigmask(SIG_UNBLOCK, &signals, nullptr);
        std::raise(status.term_signal);
    }
#endif
    return static_cast<int>(result_of(status).code);
}

kota::task<exit_status> handle_proxy_request(std::span<const std::string> argv,
                                             const catter::proxy::ProxyOption& opt,
                                             proxy::Peer& peer) noexcept {
    // ensure peer is closed when proxy_main exits, otherwise the peer might still be
    // running and trying to access resources that have been cleaned up after proxy_main
    // exits.
    auto guard = make_guard([&]() noexcept {
        auto err = peer.close();
        if(err.has_error()) {
            LOG_ERROR("Failed to close IPC peer: {}", err.error().message);
        }
    });

    std::string err;

    try {
        if(opt.error_msg.has_value() && !opt.args.has_value()) {
            co_await peer.report_error(*opt.parent_id, *opt.error_msg);
            co_return exit_status{.code = -1};
        }

        auto cmd = command_of(opt);

        if(!co_await peer.check_mode(data::ServiceMode::INJECT)) {
            throw cpptrace::runtime_error(
                "catter is not in inject mode, cannot handle the request");
        }

        auto id = co_await peer.create(*opt.parent_id);

        auto received_act = co_await peer.make_decision(cmd);

        auto status = co_await run(received_act, id, *opt.ipc, peer);

        // A command can outlive its session (a build server): its result
        // stands, though catter is gone.
        try {
            co_await peer.finish(result_of(status));
        } catch(const std::exception& ex) {
            LOG_WARN("Command {} ended after its session: {}", id, ex.what());
        }

        co_return status;
    } catch(const cpptrace::exception& ex) {
        err = std::format("{}\n", ex.message());
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    } catch(const std::exception& ex) {
        err = std::format("{}\n", ex.what());
    } catch(...) {
        err = "Unknown fatal error.\n";
    }

    auto msg = std::format("Run `{}` failed:\n{}", format_args(argv), err);

    LOG_ERROR(msg);
    co_await peer.report_error(*opt.parent_id, msg);

    co_return exit_status{.code = -1};
};

kota::task<exit_status> proxy_main(std::span<const std::string> argv,
                                   const catter::proxy::ProxyOption& opt) noexcept {
    auto& current = kota::event_loop::current();
    auto ret = co_await kota::pipe::connect(*opt.ipc, kota::pipe::options(), current);
    if(!ret) {
        // A build server an earlier session started (an sccache server, a
        // Gradle daemon) outlives that session's pipe: its commands run as
        // they would without catter.
        LOG_WARN("Failed to connect to IPC pipe: {}, error: {}; running the command uncaptured",
                 *opt.ipc,
                 ret.error().message());
        try {
            auto opts = process_options(command_of(opt));
            co_return co_await run_process(std::move(opts), {.stand_in = true});
        } catch(const std::exception& ex) {
            LOG_ERROR("Run `{}` failed: {}", format_args(argv), ex.what());
            co_return exit_status{.code = -1};
        }
    }
    auto peer = proxy::Peer{
        kota::ipc::BincodePeer{current,
                               std::make_unique<kota::ipc::StreamTransport>(std::move(*ret))}
    };

    auto [_, status] = co_await kota::when_all{peer.run(), handle_proxy_request(argv, opt, peer)};

    co_return status;
}

// we do not output in proxy, it must be invoked by main program.
// usage: catter-proxy.exe -p <parent ipc id> [--exec <exe path>] -- <args...>
// TODO: act as a fake compiler
int run(std::span<const std::string> argv) {
    try {
        log::init_logger("catter-proxy.log", data_path(config::PROXY_LOG_PATH), false);
    } catch(const std::exception&) {
        log::mute_logger();
    }

    kota::deco::cli::Command<catter::proxy::Option> cli(
        "Catter Proxy, the tool for receive hook info and send it to catter.");

    int ret = 0;
    std::vector<std::string> args(argv.begin() + 1, argv.end());
    cli.match(catter::proxy::Option::Cate::help,
              [&](const catter::proxy::Option& opt) { cli.usage(std::cerr); })
        .match(catter::proxy::Option::Cate::proxy,
               [&](const auto& opt) {
                   auto [status] = kota::run(proxy_main(argv, opt.proxy_opt));
                   ret = status.has_value() ? exit_like(*status) : -1;
               })
        .on_error([&](const kota::deco::cli::ParseError& err) {
            std::cerr << err.message << std::endl;
            ret = -1;
        })
        .execute(args);
    return ret;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef CATTER_WINDOWS
    // main's argv is in the ANSI code page, which holds few characters past
    // ASCII.
    return run(kota::sys::arguments());
#else
    return run(std::vector<std::string>(argv, argv + argc));
#endif
}
