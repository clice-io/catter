

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

    void finish(int64_t code) override {
        this->finish_called = true;
        std::println("[{}] Command finished with code: {}", this->id, code);
    }

    void report_error(data::ipcid_t parent_id, std::string error_msg) override {
        this->error_reported = true;
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
};

int main(int argc, char* argv[]) {
    // catter::log::mute_logger();
    try {
        Session session;
        Session::ProcessLaunchPlan launch_plan;
        launch_plan.executable = (util::get_catter_root_path() / config::proxy::EXE_NAME).string();
        launch_plan.args = {
            launch_plan.executable,
            "-p",
            "0",
            "--exec",
            "echo",
            "--",
            "echo",
            "Hello, World!",
        };

        std::println("Session started.");
        auto session_plan = Session::make_run_plan(std::move(launch_plan), ServiceImpl::Factory{});
        auto ret = session.run(std::move(session_plan));
        std::println("Session finished with code: {}", ret);
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
