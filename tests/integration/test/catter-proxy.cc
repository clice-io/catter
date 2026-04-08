

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
#include <stdexcept>
#include <vector>
#include <string>
#include <print>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <format>
#include <print>
#include <optional>

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

    static void reset_observed_state() {
        last_received_command.reset();
        last_error.reset();
    }

    data::ipcid_t create(data::ipcid_t parent_id) override {
        this->create_called = true;
        std::println("[{}] Creating service with parent id {}", this->id, parent_id);
        return this->id;
    }

    data::action make_decision(data::command cmd) override {
        this->make_decision_called = true;
        last_received_command = cmd;
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
    inline static std::optional<data::command> last_received_command;
    inline static std::optional<std::string> last_error;
};

namespace {

int run_case(std::vector<std::string> args, std::string cwd = {}) {
    ServiceImpl::reset_observed_state();
    Session session;
    Session::ProcessLaunchPlan launch_plan;
    launch_plan.executable = (util::get_catter_root_path() / config::proxy::EXE_NAME).string();
    launch_plan.cwd = std::move(cwd);
    launch_plan.args = std::move(args);
    auto session_plan = Session::make_run_plan(std::move(launch_plan), ServiceImpl::Factory{});
    return static_cast<int>(session.run(std::move(session_plan)));
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    try {
        auto proxy_path = (util::get_catter_root_path() / config::proxy::EXE_NAME).string();

        auto explicit_ret = run_case({
            proxy_path,
            "-p",
            "0",
            "--exec",
            "echo",
            "--",
            "echo",
            "Hello, World!",
        });

        if(explicit_ret != 0) {
            std::println("explicit --exec case failed with code {}", explicit_ret);
            return 1;
        }
        if(!ServiceImpl::last_received_command.has_value() ||
           ServiceImpl::last_received_command->executable != "echo") {
            std::println("explicit --exec case did not preserve explicit executable");
            return 1;
        }

        auto implicit_ret = run_case({
            proxy_path,
            "-p",
            "0",
            "--",
            "echo",
            "Hello, World!",
        });
        if(implicit_ret != 0) {
            std::println("implicit resolve case failed with code {}", implicit_ret);
            return 1;
        }
        if(!ServiceImpl::last_received_command.has_value() ||
           ServiceImpl::last_received_command->executable.empty() ||
           ServiceImpl::last_received_command->executable == "echo") {
            std::println("implicit resolve case did not resolve executable to a concrete path");
            return 1;
        }

        auto missing_ret = run_case({
            proxy_path,
            "-p",
            "0",
            "--",
        });
        if(missing_ret == 0) {
            std::println("missing command case unexpectedly succeeded");
            return 1;
        }
        if(!ServiceImpl::last_error.has_value() ||
           !ServiceImpl::last_error->contains("missing executable")) {
            std::println("missing command case did not report the expected error");
            return 1;
        }

        std::println("proxy integration checks passed");
        return 0;
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
}
