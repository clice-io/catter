#include <cstdint>
#include <cstdlib>
#include <eventide/async/io/loop.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <eventide/async/async.h>
#include <eventide/deco/deco.h>

#include "ipc.h"
#include "hook.h"

#include "opt/proxy/option.h"
#include "config/catter-proxy.h"
#include "shared/resolver.h"
#include "util/log.h"
#include "util/eventide.h"
#include "util/crossplat.h"
#include "util/output.h"

using namespace catter;

namespace {
using catter::data::action;

#ifndef CATTER_WINDOWS
const char* get_env_value(const std::vector<std::string>& env, std::string_view key) {
    for(const auto& entry: env) {
        auto separator = entry.find('=');
        if(separator == key.size() && entry.substr(0, separator) == key) {
            return entry.c_str() + separator + 1;
        }
    }
    return nullptr;
}
#endif

std::string resolve_executable(const catter::proxy::ProxyOption& opt,
                               const std::vector<std::string>& env) {
    if(opt.exec.has_value()) {
        return *opt.exec;
    }

    if(!opt.args.has_value() || opt.args->empty()) {
        throw std::runtime_error(
            "Missing executable: provide --exec or at least one argument after '--'.");
    }

    auto executable_token = opt.args->front();

#ifdef CATTER_WINDOWS
    auto resolved = catter::hook::shared::WindowsResolver<char>::from_current_process()
                        .resolve_command_line_token(executable_token);
    if(!resolved.has_value()) {
        throw std::runtime_error(std::format(
            "Failed to resolve executable token '{}': {}",
            executable_token,
            std::system_error(static_cast<int>(resolved.error()), std::system_category()).what()));
    }
    return std::move(*resolved);
#else
    const char* path_value = get_env_value(env, "PATH");
    std::expected<std::filesystem::path, int> resolved =
        executable_token.contains('/') ? catter::hook::shared::resolve_path_like(executable_token)
        : path_value != nullptr
            ? catter::hook::shared::resolve_from_search_path(executable_token, path_value)
            : catter::hook::shared::resolve_from_environment(executable_token, nullptr);
    if(!resolved.has_value()) {
        throw std::runtime_error(
            std::format("Failed to resolve executable token '{}': {}",
                        executable_token,
                        std::system_error(resolved.error(), std::system_category()).what()));
    }
    return resolved->string();
#endif
}

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

            return catter::capture_process_result(make_process_event(opts));
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
        if(opt.error_msg.has_value()) {
            proxy::ipc::report_error(*opt.parent_id, *opt.error_msg);
            return -1;
        }

        // This function is for the hook to call, it will never be called in this file.
        // The implementation is in hook.cc
        proxy::ipc::set_service_mode(data::ServiceMode::INJECT);

        auto env = catter::util::get_environment();
        data::command cmd = {
            .cwd = std::filesystem::current_path().string(),
            .executable = resolve_executable(opt, env),
            .args = opt.args.value_or(std::vector<std::string>{}),
            .env = std::move(env),
        };

        auto id = proxy::ipc::create(*opt.parent_id);

        auto received_act = proxy::ipc::make_decision(cmd);

        auto result = run(received_act, id);

        proxy::ipc::finish(std::move(result));

        return static_cast<int>(result.code);
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

    deco::cli::Command<catter::proxy::Option> cli(
        "Catter Proxy, the tool for receive hook info and send it to catter.");

    int ret = 0;
    auto args = deco::util::argvify(argc, argv, 1);
    cli.match(catter::proxy::Option::Cate::help,
              [&](const catter::proxy::Option& opt) { cli.usage(std::cerr); })
        .match(catter::proxy::Option::Cate::proxy,
               [&](const auto& opt) { ret = proxy_main(opt.proxy_opt); })
        .on_error([&](const deco::cli::ParseError& err) {
            std::cerr << err.message << std::endl;
            ret = -1;
        })
        .execute(args);
    return ret;
}
