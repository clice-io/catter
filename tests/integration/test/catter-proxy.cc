

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
        LOG_INFO("[{}] Creating service with parent id {}", this->id, parent_id);
        return this->id;
    }

    data::action make_decision(data::command cmd) override {
        this->make_decision_called = true;
        last_received_command = cmd;
        std::string args_str;
        for(const auto& arg: cmd.args) {
            args_str.append(arg).append(" ");
        }

        LOG_INFO("[{}] Received command: \n    -> cwd = {} \n    -> exe = {} \n    -> args = {}",
                 this->id,
                 cmd.cwd,
                 cmd.executable,
                 args_str);
        return data::action{.type = data::action::WRAP, .cmd = cmd};
    }

    void finish(data::process_result result) override {
        this->finish_called = true;
        LOG_INFO(
            "[{}] Command finished: \n    -> code = {}\n    -> stdout = `{}` \n    -> stderr = `{}`",
            this->id,
            result.code,
            log::escape(result.std_out),
            log::escape(result.std_err));
    }

    void report_error(data::ipcid_t parent_id, std::string error_msg) override {
        this->error_reported = true;
        last_error = error_msg;
        LOG_INFO("[{}] Error reported for command with parent id {} : {}",
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
    Session::ProcessLaunchPlan launch_plan{
        .cwd = std::move(cwd),
        .executable = (util::get_catter_root_path() / config::proxy::EXE_NAME).string(),
        .args = std::move(args),
    };
    auto session_plan = Session::make_run_plan(std::move(launch_plan), ServiceImpl::Factory{});
    return static_cast<int>(session.run(std::move(session_plan)));
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    try {
        auto proxy_path = (util::get_catter_root_path() / config::proxy::EXE_NAME).string();
        auto& last_cmd = ServiceImpl::last_received_command;
        auto& last_err = ServiceImpl::last_error;
        assert(run_case({
                   proxy_path,
                   "-p",
                   "0",
                   "--exec",
                   "echo",
                   "--",
                   "echo",
                   "Hello, World!",
               }) == 0 &&
               "explicit --exec case failed");
        assert(last_cmd.has_value() && last_cmd->executable == "echo" &&
               "explicit --exec case did not preserve explicit executable");

        assert(run_case({
                   proxy_path,
                   "-p",
                   "0",
                   "--",
                   "echo",
                   "Hello, World!",
               }) == 0 &&
               "implicit resolve case failed");
        assert(last_cmd.has_value() && last_cmd->executable != "echo" &&
               "implicit resolve case did not resolve executable to a concrete path");

        assert(run_case({proxy_path, "-p", "0", "--"}) != 0 &&
               "implicit resolve with missing executable case unexpectedly succeeded");
        assert(last_err.has_value() &&
               "implicit resolve with missing executable case did not report the expected error");
        assert(!last_cmd.has_value() &&
               "implicit resolve with missing executable case unexpectedly produced a command");

        assert(run_case({
                   proxy_path,
                   "-p",
                   "0",
                   "--",
                   "nonexistent_executable_12345",
               }) != 0 &&
               "implicit resolve with nonexistent executable case unexpectedly succeeded");
        assert(
            last_err.has_value() &&
            "implicit resolve with nonexistent executable case did not report the expected error");

        LOG_INFO("proxy integration checks passed");
        return 0;
    } catch(const std::exception& ex) {
        LOG_INFO("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        LOG_INFO("Unknown fatal error.");
        return 1;
    }
}
