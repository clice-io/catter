module;

#include "modules/prelude.h"

#include "support/config/catter-proxy.h"
#include "support/config/ipc.h"
#include "support/guard.h"
#include "support/log.h"
#include "support/output.h"
#include "support/paths.h"

module catter;

import :capture.inject.hook;
import :proxy.ipc;
import :proxy.option;
import :session.process;

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
    auto exited = make_guard([&]() noexcept { peer.program.reset(); });
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

/// The command the proxy stands in for, as it runs here. Its working
/// directory is empty when it cannot be read, as when the build removed it:
/// the program starts in it all the same.
data::command command_of(const catter::proxy::ProxyOption& opt) {
    if(!opt.args.has_value()) {
        throw cpptrace::runtime_error("missing command arguments after --");
    }

    data::command cmd = {.args = *opt.args};
    if(kota::fs::current_dir(cmd.cwd)) {
        cmd.cwd.clear();
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
        if(cmd.cwd.empty()) {
            co_await peer.report_error(
                *opt.parent_id,
                std::format(
                    "`{}` runs in a working directory catter cannot read, which the build may have removed: it runs uncaptured",
                    format_args(cmd.args)));
            co_return co_await run_process(process_options(cmd), {.stand_in = true});
        }

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

#ifdef CATTER_MAC
/// Runs the connection to catter. macOS ends no child with its parent: the
/// build command ends here when catter ends while it runs, and with catter its
/// connection.
kota::task<> run_connection(proxy::Peer& peer, bool build_command) {
    co_await peer.run();
    if(build_command && peer.program) {
        ::kill(static_cast<pid_t>(*peer.program), SIGKILL);
    }
}
#endif

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

#ifdef CATTER_MAC
    // The build command's proxy is started with parent 0.
    auto connection = run_connection(peer, *opt.parent_id == 0);
#else
    auto connection = peer.run();
#endif
    auto [_, status] =
        co_await kota::when_all{std::move(connection), handle_proxy_request(argv, opt, peer)};

    co_return status;
}

// we do not output in proxy, it must be invoked by main program.
// usage: catter-proxy.exe -p <parent ipc id> [--exec <exe path>] -- <args...>
// TODO: act as a fake compiler
int run(std::span<const std::string> argv) {
    hold_closed_streams();
    kota::deco::cli::Command<catter::proxy::Option> cli(
        "Catter Proxy, the tool for receive hook info and send it to catter.");

    int ret = 0;
    std::vector<std::string> args(argv.begin() + 1, argv.end());
    cli.match(catter::proxy::Option::Cate::help,
              [&](const catter::proxy::Option& opt) { cli.usage(std::cerr); })
        .match(catter::proxy::Option::Cate::proxy,
               [&](const auto& opt) {
                   try {
                       log::init_logger(
                           "catter-proxy.log",
                           config::session_data_path(*opt.proxy_opt.ipc, config::PROXY_LOG_PATH),
                           false);
                   } catch(const std::exception&) {
                       log::mute_logger();
                   }
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

extern "C++" int main(int argc, char* argv[]) {
#ifdef CATTER_WINDOWS
    // main's argv is in the ANSI code page, which holds few characters past
    // ASCII.
    return run(kota::sys::arguments());
#else
    return run(std::vector<std::string>(argv, argv + argc));
#endif
}
