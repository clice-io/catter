#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <kota/async/async.h>
#include <kota/deco/deco.h>

#include "hook.h"
#include "ipc.h"
#include "config/catter-proxy.h"
#include "opt/proxy/option.h"
#include "shared/resolver.h"
#include "util/crossplat.h"
#include "util/guard.h"
#include "util/kotatsu.h"
#include "util/log.h"
#include "util/output.h"

using namespace catter;

namespace {
using catter::data::action;

std::string resolve_executable(std::string_view exe, const std::vector<std::string>& env) {

#ifdef CATTER_WINDOWS
    return catter::hook::shared::resolver::resolve_command_line_token<char>(exe);
#else
    std::string path_env;
    for(const auto& env_var: env) {
        if(env_var.starts_with("PATH=")) {
            path_env = env_var.substr(5);
            break;
        }
    }
    if(path_env.empty()) {
        throw std::runtime_error("PATH environment variable not found");
    }
    auto resolved = catter::hook::shared::resolver::resolve_from_path_env(exe, path_env.c_str());
    if(!resolved.has_value()) {
        // if not found, just return the original string and let the system handle it, which will
        // produce the same error as if we did not resolve it.
        return std::string(exe);
    }
    return resolved->string();
#endif
}

kota::task<data::process_result> run(data::action act, data::ipcid_t id) {
    using catter::data::action;

    switch(act.type) {
        case action::WRAP: {
            kota::process::options opts{
                .file = act.cmd.executable,
                .args = act.cmd.args,
                .env = act.cmd.env,
                .cwd = act.cmd.cwd,
                .creation = {.windows_hide = true, .windows_verbatim_arguments = true},
                .streams = {kota::process::stdio::inherit(),
                             kota::process::stdio::pipe(false, true),
                             kota::process::stdio::pipe(false, true)}
            };
            co_return co_await capture_process_result(make_process_event(opts));
        }
        case action::INJECT: {
            co_return co_await proxy::hook::run(act.cmd, id);
        }
        case action::DROP: {
            co_return data::process_result{.code = 0};
        }
        default: {
            co_return data::process_result{.code = -1};
        }
    }
}

kota::task<int> proxy_main(const catter::proxy::ProxyOption& opt) noexcept {
    auto& current = kota::event_loop::current();
    auto ret =
        co_await kota::pipe::connect(config::ipc::pipe_name(), kota::pipe::options(), current);
    if(!ret) {
        LOG_CRITICAL("Failed to connect to IPC pipe: {}, error: {}",
                     config::ipc::pipe_name(),
                     ret.error().message());
        std::abort();
    }
    auto peer = proxy::ipc::Peer{
        kota::ipc::BincodePeer{current,
                               std::make_unique<kota::ipc::StreamTransport>(std::move(*ret))}
    };

    auto [code, _] = co_await kota::when_all{
        [](const catter::proxy::ProxyOption& opt,
           proxy::ipc::Peer& peer) noexcept -> kota::task<int> {
            // ensure peer is closed when proxy_main exits, otherwise the peer might still be
            // running and trying to access resources that have been cleaned up after proxy_main
            // exits.
            auto guard = util::make_guard([&]() noexcept {
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

                if(!opt.args.has_value()) {
                    throw std::runtime_error("missing command arguments after --");
                }

                if(!co_await peer.check_mode(data::ServiceMode::INJECT)) {
                    throw std::runtime_error(
                        "catter is not in inject mode, cannot handle the request");
                }

                data::command cmd = {
                    .cwd = std::filesystem::current_path().string(),
                    .args = *opt.args,
                    .env = catter::util::get_environment(),
                };

                if(opt.exec.has_value()) {
                    cmd.executable = *opt.exec;
                } else {
                    cmd.executable = resolve_executable(cmd.args.at(0), cmd.env);
                }

                auto id = co_await peer.create(*opt.parent_id);

                auto received_act = co_await peer.make_decision(cmd);

                auto result = co_await run(received_act, id);

                co_await peer.finish(std::move(result));

                co_return static_cast<int>(result.code);
            } catch(const std::exception& e) {
                std::string args;
                if(opt.args.has_value()) {
                    args.reserve(opt.args->size() * 5);
                    for(int i = 0; i < opt.args->size(); ++i) {
                        args += ' ';
                        args += (*opt.args)[i];
                    }
                }
                LOG_CRITICAL("Exception in catter-proxy: {}. Args: {}", e.what(), args);
                err = e.what();
            } catch(...) {
                LOG_CRITICAL("Unknown exception in catter-proxy.");
                err = "Unknown exception in catter-proxy.";
            }
            co_await peer.report_error(*opt.parent_id, err);
            co_return -1;
        }(opt, peer),
        peer.run()};
    co_return code;
}
}  // namespace

// we do not output in proxy, it must be invoked by main program.
// usage: catter-proxy.exe -p <parent ipc id> [--exec <exe path>] -- <args...>
// TODO: act as a fake compiler
int main(int argc, char* argv[], [[maybe_unused]] char* envp[]) {
    try {
        log::init_logger("catter-proxy.log",
                         util::get_catter_data_path() / config::proxy::LOG_PATH_REL,
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
               [&](const auto& opt) { ret = catter::wait(proxy_main(opt.proxy_opt)); })
        .on_error([&](const kota::deco::cli::ParseError& err) {
            std::cerr << err.message << std::endl;
            ret = -1;
        })
        .execute(args);
    return ret;
}
