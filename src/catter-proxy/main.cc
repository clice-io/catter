#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <eventide/async/async.h>
#include <eventide/deco/runtime.h>

#include "ipc.h"
#include "hook.h"

#include "opt/proxy/option.h"
#include "config/catter-proxy.h"
#include "util/log.h"
#include "util/eventide.h"
#include "util/crossplat.h"
#include "util/output.h"

using namespace catter;

namespace {
using catter::data::action;

data::process_result run(data::action act, data::ipcid_t id) {
    using catter::data::action;
    switch(act.type) {
        case action::WRAP: {
            eventide::process::options opts{
                .file = act.cmd.executable,
                .args = act.cmd.args,
                .env = act.cmd.env,
                .cwd = act.cmd.cwd,
                .creation = {.windows_hide = true, .windows_verbatim_arguments = true},
                .streams = {eventide::process::stdio::inherit(),
                             eventide::process::stdio::pipe(false, true),
                             eventide::process::stdio::pipe(false, true)}
            };
            auto spawn_ret = eventide::process::spawn(opts, catter::default_loop());
            if(!spawn_ret) {
                throw std::runtime_error(
                    std::format("process spawn failed: {}", spawn_ret.error().message()));
            }
            return catter::capture_process_result(
                [](eventide::process proc) -> eventide::task<int64_t, eventide::error> {
                    auto wait_ret = co_await proc.wait();
                    if(!wait_ret) {
                        co_return eventide::outcome_error(wait_ret.error());
                    }
                    co_return wait_ret->status;
                }(std::move(spawn_ret->proc)),
                std::move(spawn_ret->stdout_pipe),
                std::move(spawn_ret->stderr_pipe),
                stdout,
                stderr);
        }
        case action::INJECT: {
            return proxy::hook::run(act.cmd, id);
        }
        case action::DROP: {
            return data::process_result{.code = 0};
        }
        default: {
            return data::process_result{.code = -1};
        }
    }
}

int proxy_main(const catter::proxy::ProxyOption& opt) {
    try {
        // This function is for the hook to call, it will never be called in this file.
        // The implementation is in hook.cc
        proxy::ipc::set_service_mode(data::ServiceMode::INJECT);

        data::command cmd = {
            .cwd = std::filesystem::current_path().string(),
            .executable = *opt.exec,
            .args = *opt.args,
            .env = catter::util::get_environment(),
        };

        auto id = proxy::ipc::create(*opt.parent_id);

        auto received_act = proxy::ipc::make_decision(cmd);

        auto result = run(received_act, id);

        proxy::ipc::finish(std::move(result));

        return static_cast<int>(result.code);
    } catch(const std::exception& e) {
        std::string args;
        args.reserve(opt.args->size() * 5);
        for(int i = 0; i < opt.args->size(); ++i) {
            args += ' ';
            args += (*opt.args)[i];
        }

        LOG_CRITICAL("Exception in catter-proxy: {}. Args: {}", e.what(), args);
        proxy::ipc::report_error(*opt.parent_id, e.what());
        return -1;
    } catch(...) {
        LOG_CRITICAL("Unknown exception in catter-proxy.");
        proxy::ipc::report_error(*opt.parent_id, "Unknown exception in catter-proxy.");
        return -1;
    }
}
}  // namespace

// we do not output in proxy, it must be invoked by main program.
// usage: catter-proxy.exe -p <parent ipc id> --exec <exe path> -- <args...>
// TODO: act as a fake compiler
int main(int argc, char* argv[], [[maybe_unused]] char* envp[]) {
    try {
        log::init_logger("catter-proxy.log",
                         util::get_catter_data_path() / config::proxy::LOG_PATH_REL,
                         false);
    } catch(const std::exception&) {
        log::mute_logger();
    }

    deco::cli::Dispatcher<catter::proxy::Option> cli(
        "Catter Proxy, the tool for receive hook info and send it to catter.");

    int ret = 0;
    auto args = deco::util::argvify(argc, argv, 1);
    cli.dispatch(catter::proxy::Option::Cate::help,
                 [&]([[maybe_unused]] const catter::proxy::Option& opt) { cli.usage(std::cerr); })
        .dispatch(catter::proxy::Option::Cate::proxy,
                  [&](const auto& opt) { ret = proxy_main(opt.proxy_opt); })
        .when_err([&](const deco::cli::ParseError& err) {
            std::cerr << err.message << std::endl;
            ret = -1;
        })
        .parse(args);
    return ret;
}
