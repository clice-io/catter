#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <print>
#include <cassert>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <fstream>

#include <eventide/async/async.h>
#include <eventide/reflection/name.h>
#include <eventide/deco/runtime.h>

#include "capi/type.h"
#include "opt/main/option.h"

#include "js.h"
#include "ipc.h"
#include "qjs.h"
#include "session.h"
#include "util/crossplat.h"
#include "util/data.h"
#include "util/log.h"
#include "config/catter.h"

using namespace catter;

class ServiceImpl : public ipc::InjectService {
public:
    ServiceImpl(data::ipcid_t id, const js::CatterRuntime* runtime) : id(id), runtime(runtime) {};
    ~ServiceImpl() override = default;

    data::ipcid_t create(data::ipcid_t parent_id) override {
        this->parent_id = parent_id;
        return this->id;
    }

    data::action make_decision(data::command cmd) override {
        auto act = js::on_command(this->id,
                                  js::CommandData{
                                      .cwd = cmd.cwd,
                                      .exe = cmd.executable,
                                      .argv = cmd.args,
                                      .env = cmd.env,
                                      .runtime = *runtime,
                                      .parent = this->parent_id,
                                  });

        switch(act.type()) {
            case js::ActionType::drop: {
                return data::action{.type = data::action::DROP, .cmd = {}};
            }
            case js::ActionType::skip: {
                return data::action{.type = data::action::INJECT, .cmd = cmd};
            }

            case js::ActionType::modify: {
                auto& tag = act.get<js::ActionType::modify>();

                return data::action{
                    .type = data::action::INJECT,
                    .cmd = {
                            .cwd = std::move(tag.data.cwd),
                            .executable = std::move(tag.data.exe),
                            .args = std::move(tag.data.argv),
                            .env = std::move(tag.data.env),
                            }
                };
            }
            default: throw std::runtime_error("Unhandled action type");
        }
    }

    void finish(int64_t code) override {
        js::on_execution(this->id, js::Tag<js::EventType::finish>{.code = code});
    }

    void report_error(data::ipcid_t parent_id, std::string error_msg) override {
        js::on_command(id, std::unexpected(js::CatterErr{.msg = std::move(error_msg)}));
    }

    struct Factory {
        const js::CatterRuntime* runtime;

        std::unique_ptr<ServiceImpl> operator() (data::ipcid_t id) {
            return std::make_unique<ServiceImpl>(id, runtime);
        }
    };

private:
    data::ipcid_t id = 0;
    data::ipcid_t parent_id = 0;
    const js::CatterRuntime* runtime = nullptr;
};

struct Config {
    bool log;
    ipc::ServiceMode mode;
    std::string script_path;
    std::vector<std::string> script_args;
    std::vector<std::string> build_system_command;
    std::filesystem::path working_dir;
    const js::CatterRuntime* runtime;
};

Config extract_config(const core::Option::CatterOption& opt) {
    struct mode_meta {
        ipc::ServiceMode mode;
        js::CatterRuntime runtime;
    };

    static std::unordered_map<std::string_view, mode_meta> mode_map = {
        {"inject",
         {.mode = ipc::ServiceMode::INJECT,
          .runtime = {
              .supportActions = {js::ActionType::drop,
                                 js::ActionType::skip,
                                 js::ActionType::modify},
              .supportEvents = {js::EventType::finish},
              .type = js::CatterRuntime::Type::inject,
              .supportParentId = true,
          }}}
    };

    Config config{
        .log = true,
        .script_path = *opt.script_path,
        .script_args = opt.script_args.has_value() ? *opt.script_args : std::vector<std::string>{},
        .build_system_command = *opt.args,
        .working_dir = opt.working_dir.has_value() ? std::filesystem::absolute(*opt.working_dir)
                                                   : std::filesystem::current_path(),
    };

    if(auto it = mode_map.find(*opt.mode); it != mode_map.end()) {
        config.mode = it->second.mode;
        config.runtime = &it->second.runtime;
    } else {
        throw std::runtime_error(std::format("Unsupported mode: {}", *opt.mode));
    }
    return config;
}

void inject(const Config& config) {
    if(config.script_path == "script::cdb") {
        std::string_view script = R"(
    import { scripts, service } from "catter";
    service.register(new scripts.CDB());
    )";
        js::run_js_file(script, config.script_path);
    } else {
        std::ifstream ifs{config.script_path};
        if(!ifs.good()) {
            throw std::runtime_error(
                std::format("Failed to open script file: {}", config.script_path));
        }

        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        catter::js::run_js_file(content, config.script_path);
    }

    auto new_config = js::on_start({
        .scriptPath = config.script_path,
        .scriptArgs = config.script_args,
        .buildSystemCommand = config.build_system_command,
        .runtime = *config.runtime,
        .options =
            {
                      .log = config.log,
                      },
        .isScriptSupported = true
    });

    Session session;

    auto ret =
        session.run(new_config.buildSystemCommand, ServiceImpl::Factory{.runtime = config.runtime});

    js::on_finish(js::Tag<js::EventType::finish>{
        .code = ret,
    });
}

void dispatch(const core::Option::CatterOption& opt) {
    auto config = extract_config(opt);

    js::init_qjs({.pwd = config.working_dir});

    switch(config.mode) {
        case ipc::ServiceMode::INJECT: {
            inject(config);
            break;
        }
        default: {
            throw std::runtime_error(std::format("UnExpected mode: {:0x}", (uint8_t)config.mode));
        }
    }
}

int main(int argc, char* argv[]) {
    auto args = deco::util::argvify(argc, argv, 1);

    try {
        log::init_logger("catter", util::get_catter_data_path() / config::core::LOG_PATH_REL);
        deco::cli::Dispatcher<core::Option> cli("catter [options] -- [build system command]");
        cli.dispatch(core::Option::HelpOpt::category_info,
                     [&](const core::Option& opt) { cli.usage(std::cout); })
            .dispatch(core::Option::CatterOption::category_info,
                      [&](const auto& opt) { dispatch(opt.main_opt); })
            .dispatch([&](const auto&) { cli.usage(std::cout); })
            .when_err([&](const deco::cli::ParseError& err) {
                std::println("Error parsing options: {}", err.message);
                std::println("Use -h or --help for usage.");
            })
            .parse(args);
    } catch(const qjs::JSException& ex) {
        std::println("Eval JavaScript file failed: \n{}", ex.what());
        return 1;
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}
