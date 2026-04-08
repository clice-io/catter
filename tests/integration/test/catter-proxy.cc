

// clang-format off
// RUN: %it_catter_proxy
// clang-format on
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
#include <string>
#include <print>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <format>
#include <print>

#include <eventide/async/async.h>
#include <eventide/reflection/name.h>

#include "session.h"
#include "ipc.h"
#include "config/ipc.h"
#include "config/catter-proxy.h"
#include "util/crossplat.h"
#include "util/eventide.h"
#include "util/serde.h"
#include "util/data.h"
#include "util/log.h"

using namespace catter;

class ServiceImpl : public ipc::InjectService {
public:
    ServiceImpl(data::ipcid_t id) : id(id) {};
    ~ServiceImpl() override = default;

    data::ipcid_t create(data::ipcid_t parent_id) override {
        this->create_called = true;
        std::println("[{}] Creating service with parent id {}", this->id, parent_id);
        return this->id;
    }

    data::action make_decision(data::command cmd) override {
        this->make_decision_called = true;
        last_command = cmd;
        std::string args_str;
        for(const auto& arg: cmd.args) {
            args_str.append(arg).append(" ");
        }

        std::println(
            "[{}] Received command: \n    -> cwd = {} \n    -> exe = {} \n    -> args = {}",
            this->id,
            cmd.cwd,
            cmd.executable,
            args_str);
        return data::action{.type = data::action::WRAP, .cmd = cmd};
    }

    void finish(data::process_result result) override {
        this->finish_called = true;
        std::println(
            "[{}] Command finished: \n    -> code = {}\n    -> stdout = `{}` \n    -> stderr = `{}`",
            this->id,
            result.code,
            log::escape(result.std_out),
            log::escape(result.std_err));
    }

    void report_error(data::ipcid_t parent_id, std::string error_msg) override {
        this->error_reported = true;
        last_error = error_msg;
        std::println("[{}] Error reported for command with parent id {} : {}",
                     this->id,
                     parent_id,
                     error_msg);
    }

    struct Factory {
        std::unique_ptr<ServiceImpl> operator() (data::ipcid_t id) const {
            return std::make_unique<ServiceImpl>(id);
        }
    };

public:
    bool create_called = false;
    bool make_decision_called = false;
    bool finish_called = false;
    bool error_reported = false;
    data::ipcid_t id;
    inline static std::optional<data::command> last_command = std::nullopt;
    inline static std::optional<std::string> last_error = std::nullopt;
};

int run_case(Session::ProcessLaunchPlan launch_plan) {
    ServiceImpl::last_command.reset();
    ServiceImpl::last_error.reset();

    Session session;
    auto session_plan = Session::make_run_plan(std::move(launch_plan), ServiceImpl::Factory{});
    return session.run(std::move(session_plan));
}

int main(int argc, char* argv[]) {
    // catter::log::mute_logger();
    try {
        auto proxy_executable = (util::get_catter_root_path() / config::proxy::EXE_NAME).string();

        Session::ProcessLaunchPlan explicit_plan{
            .executable = proxy_executable,
            .args =
                {
                       proxy_executable, "-p",
                       "0", "--exec",
#ifdef CATTER_WINDOWS
                       "cmd.exe", "--",
                       "cmd.exe", "/c",
                       "echo Hello, World!",
#else
                    "/bin/echo",
                    "--",
                    "/bin/echo",
                    "Hello, World!",
#endif
                       },
        };
        auto ret = run_case(std::move(explicit_plan));
        if(ret != 0 || !ServiceImpl::last_command.has_value()) {
            return 1;
        }

        Session::ProcessLaunchPlan implicit_plan{
            .executable = proxy_executable,
            .args =
                {
                       proxy_executable, "-p",
                       "0", "--",
#ifdef CATTER_WINDOWS
                       "cmd", "/c",
                       "echo Hello, World!",
#else
                    "echo",
                    "Hello, World!",
#endif
                       },
        };
        ret = run_case(std::move(implicit_plan));
        if(ret != 0 || !ServiceImpl::last_command.has_value()) {
            return 1;
        }
#ifdef CATTER_WINDOWS
        if(ServiceImpl::last_command->executable.find("cmd") == std::string::npos) {
            return 1;
        }
#else
        if(ServiceImpl::last_command->executable != "/usr/bin/echo" &&
           ServiceImpl::last_command->executable != "/bin/echo") {
            return 1;
        }
#endif

        Session::ProcessLaunchPlan error_plan{
            .executable = proxy_executable,
            .args = {proxy_executable, "-p", "0", "--"},
        };
        ret = run_case(std::move(error_plan));
        if(ret == 0 || !ServiceImpl::last_error.has_value()) {
            return 1;
        }

        return 0;
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}
