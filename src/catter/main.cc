#include <functional>
#include <print>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <format>
#include <print>

#include <eventide/async/process.h>
#include <eventide/async/stream.h>
#include <eventide/async/loop.h>
#include <eventide/reflection/name.h>

#include "js.h"
#include "ipc.h"
#include "session.h"

using namespace catter;

class ServiceImpl : public ipc::DefaultService {
public:
    ServiceImpl(data::ipcid_t id) : id(id) {};
    ~ServiceImpl() override = default;

    data::ipcid_t create(data::ipcid_t parent_id) override {
        std::println("[{}] Creating service with parent id {}", this->id, parent_id);
        return this->id;
    }

    data::action make_decision(data::command cmd) override {
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
        std::println("[{}] Command finished with code: {}", this->id, code);
    }

    void report_error(data::ipcid_t parent_id, std::string error_msg) override {
        std::println("[{}] Error reported for command with parent id {} : {}",
                     this->id,
                     parent_id,
                     error_msg);
    }

    struct Factory {
        std::unique_ptr<ServiceImpl> operator() (data::ipcid_t id) {
            return std::make_unique<ServiceImpl>(id);
        }
    };

private:
    data::ipcid_t id;
};

class SessionImpl : public Session {
public:
    bool start(data::ServiceMode mode) override {
        std::println("Session started ");
        return true;
    }

    void finish(int64_t code) override {
        std::println("Session finished with code: {}", code);
    }
};

int main(int argc, char* argv[]) {

    if(argc < 2 || std::string(argv[1]) != "--") {
        std::println("Usage: catter -- <target program> [args...]");
        return 1;
    }

    std::vector<std::string> shell;

    for(int i = 2; i < argc; ++i) {
        shell.push_back(argv[i]);
    }

    try {
        SessionImpl session;

        session.run(shell, ServiceImpl::Factory{});
    } catch(const std::exception& ex) {
        std::println("Fatal error: {}", ex.what());
        return 1;
    } catch(...) {
        std::println("Unknown fatal error.");
        return 1;
    }
    return 0;
}
