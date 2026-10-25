#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/async/async.h>
#include <kota/deco/deco.h>

#include "capture/inject/hook.h"
#include "proxy/ipc.h"
#include "proxy/option.h"
#include "session/process.h"
#include "support/config/catter-proxy.h"
#include "support/crossplat.h"
#include "support/encoding.h"
#include "support/guard.h"
#include "support/log.h"
#include "support/output.h"
#include "support/resolver/resolver.h"

using namespace catter;

namespace {
using catter::data::action;

std::string resolve_executable(std::string_view exe, const std::vector<std::string>& env) {

#ifdef CATTER_WINDOWS
    return from_wide(catter::resolve_command_line_token<wchar_t>(to_wide(exe)));
#else
    std::string path_env;
    for(const auto& env_var: env) {
        if(env_var.starts_with("PATH=")) {
            path_env = env_var.substr(5);
            break;
        }
    }
    if(path_env.empty()) {
        throw cpptrace::runtime_error("PATH environment variable not found");
    }
    auto resolved = catter::resolve_from_path_env(exe, path_env.c_str());
    if(!resolved.has_value()) {
        // if not found, just return the original string and let the system handle it, which will
        // produce the same error as if we did not resolve it.
        return std::string(exe);
    }
    return to_utf8(*resolved);
#endif
}

kota::process::options process_options(const data::command& cmd, bool verbatim_arguments) {
    return {
        .file = cmd.executable,
        .args = cmd.args,
        .env = cmd.env,
        .cwd = cmd.cwd,
        .creation = {.windows_hide = true, .windows_verbatim_arguments = verbatim_arguments},
    };
}

kota::task<data::process_result> run(data::action act, data::ipcid_t id, std::string pipe_name) {
    using catter::data::action;

    switch(act.type) {
        case action::WRAP: {
            auto opts = process_options(act.cmd, true);
            co_return co_await run_process(opts);
        }
        case action::INJECT: {
            co_return co_await run_with_hook(act.cmd, id, std::move(pipe_name));
        }
        case action::DROP: {
            co_return data::process_result{.code = 0};
        }
        case action::FAIL: {
            co_return data::process_result{.code = 1};
        }
        default: {
            co_return data::process_result{.code = -1};
        }
    }
}

/// The command the proxy stands in for, as it runs here.
data::command command_of(const catter::proxy::ProxyOption& opt) {
    if(!opt.args.has_value()) {
        throw cpptrace::runtime_error("missing command arguments after --");
    }

    data::command cmd = {
        .cwd = to_utf8(std::filesystem::current_path()),
        .args = *opt.args,
        .env = catter::get_environment(),
    };

    if(opt.exec.has_value()) {
        cmd.executable = *opt.exec;
    } else {
        cmd.executable = resolve_executable(cmd.args.at(0), cmd.env);
    }
    return cmd;
}

std::string format_args(int argc, char* argv[]) {
    std::string result = argv[0];
    for(int i = 1; i < argc; i++) {
        result += ' ';
        result += argv[i];
    }
    return result;
}

kota::task<int> handle_proxy_request(int argc,
                                     char* argv[],
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
            co_return -1;
        }

        auto cmd = command_of(opt);

        if(!co_await peer.check_mode(data::ServiceMode::INJECT)) {
            throw cpptrace::runtime_error(
                "catter is not in inject mode, cannot handle the request");
        }

        auto id = co_await peer.create(*opt.parent_id);

        auto received_act = co_await peer.make_decision(cmd);

        auto result = co_await run(received_act, id, *opt.ipc);
        auto code = static_cast<int>(result.code);

        // A command can outlive its session (a build server): its result
        // stands, though catter is gone.
        try {
            co_await peer.finish(std::move(result));
        } catch(const std::exception& ex) {
            LOG_WARN("Command {} ended after its session: {}", id, ex.what());
        }

        co_return code;
    } catch(const cpptrace::exception& ex) {
        err = std::format("{}\n", ex.message());
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    } catch(const std::exception& ex) {
        err = std::format("{}\n", ex.what());
    } catch(...) {
        err = "Unknown fatal error.\n";
    }

    auto msg = std::format("Run `{}` failed:\n{}", format_args(argc, argv), err);

    LOG_ERROR(msg);
    co_await peer.report_error(*opt.parent_id, msg);

    co_return -1;
};

kota::task<int> proxy_main(int argc, char* argv[], const catter::proxy::ProxyOption& opt) noexcept {
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
            // The arguments are the ones the hook took apart: quoted back,
            // not joined as they are.
            auto opts = process_options(command_of(opt), false);
            auto result = co_await run_process(opts);
            co_return static_cast<int>(result.code);
        } catch(const std::exception& ex) {
            LOG_ERROR("Run `{}` failed: {}", format_args(argc, argv), ex.what());
            co_return -1;
        }
    }
    auto peer = proxy::Peer{
        kota::ipc::BincodePeer{current,
                               std::make_unique<kota::ipc::StreamTransport>(std::move(*ret))}
    };

    auto [_, code] =
        co_await kota::when_all{peer.run(), handle_proxy_request(argc, argv, opt, peer)};

    co_return code;
}

// we do not output in proxy, it must be invoked by main program.
// usage: catter-proxy.exe -p <parent ipc id> [--exec <exe path>] -- <args...>
// TODO: act as a fake compiler
int run(int argc, char* argv[]) {
    try {
        log::init_logger("catter-proxy.log",
                         get_catter_data_path() / config::PROXY_LOG_PATH,
                         false);
    } catch(const std::exception&) {
        log::mute_logger();
    }

    kota::deco::cli::Command<catter::proxy::Option> cli(
        "Catter Proxy, the tool for receive hook info and send it to catter.");

    int ret = 0;
    auto args = kota::deco::util::argvify(argc, argv, 1);
    cli.match(catter::proxy::Option::Cate::help,
              [&](const catter::proxy::Option& opt) { cli.usage(std::cerr); })
        .match(catter::proxy::Option::Cate::proxy,
               [&](const auto& opt) {
                   auto [code] = kota::run(proxy_main(argc, argv, opt.proxy_opt));
                   ret = code.has_value() ? *code : -1;
               })
        .on_error([&](const kota::deco::cli::ParseError& err) {
            std::cerr << err.message << std::endl;
            ret = -1;
        })
        .execute(args);
    return ret;
}

}  // namespace

#ifdef CATTER_WINDOWS
int wmain(int argc, wchar_t* argv[]) {
    Utf8Arguments arguments(argc, argv);
    return run(argc, arguments.argv());
}
#else
int main(int argc, char* argv[]) {
    return run(argc, argv);
}
#endif
