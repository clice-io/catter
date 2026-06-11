// clang-format off
// RUN: "%it_catter_proxy" "%catter_proxy" -p 0 --exec it-catter-proxy -- it-catter-proxy --child | FileCheck %s --check-prefix=EXPLICIT
// RUN: "%it_catter_proxy" "%catter_proxy" -p 0 -- it-catter-proxy --child | FileCheck %s --check-prefix=IMPLICIT
// RUN: not "%it_catter_proxy" "%catter_proxy" -p 0 | FileCheck %s --check-prefix=MISSING
// RUN: not "%it_catter_proxy" "%catter_proxy" -p 0 -- nonexistent-executable-catter-proxy-test | FileCheck %s --check-prefix=NONEXISTENT
//
// EXPLICIT: event=create service=1 parent=0
// EXPLICIT-NEXT: event=decision executable="it-catter-proxy" cwd="{{.*}}" argc=2
// EXPLICIT-NEXT: event=argument index=0 value="it-catter-proxy"
// EXPLICIT-NEXT: event=argument index=1 value="--child"
// EXPLICIT-NEXT: event=finish code=0 stdout="child output" stderr=""
// EXPLICIT-NEXT: proxy=exit code=0 stdout="child output" stderr=""
//
// IMPLICIT: event=create service=1 parent=0
// IMPLICIT-NEXT: event=decision executable="{{.*[/\\]}}it-catter-proxy{{(.exe)?}}" cwd="{{.*}}" argc=2
// IMPLICIT-NEXT: event=argument index=0 value="it-catter-proxy"
// IMPLICIT-NEXT: event=argument index=1 value="--child"
// IMPLICIT-NEXT: event=finish code=0 stdout="child output" stderr=""
// IMPLICIT-NEXT: proxy=exit code=0 stdout="child output" stderr=""
//
// MISSING-NOT: event=create
// MISSING-NOT: event=decision
// MISSING-NOT: event=finish
// MISSING: event=error parent=0 message="missing command arguments after --{{.*}}"
// MISSING-NOT: event=create
// MISSING-NOT: event=decision
// MISSING-NOT: event=finish
// MISSING-NEXT: proxy=exit code={{(-1|255|4294967295)}} stdout="" stderr=""
//
// NONEXISTENT: event=create service=1 parent=0
// NONEXISTENT-NEXT: event=decision executable="nonexistent-executable-catter-proxy-test" cwd="{{.*}}" argc=1
// NONEXISTENT-NEXT: event=argument index=0 value="nonexistent-executable-catter-proxy-test"
// NONEXISTENT-NEXT: event=error parent=0 message="{{.+}}"
// NONEXISTENT-NOT: event=finish
// NONEXISTENT-NEXT: proxy=exit code={{(-1|255|4294967295)}} stdout="" stderr=""
// clang-format on
#include <exception>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <vector>
#include <kota/async/async.h>

#include "ipc.h"
#include "session.h"
#include "util/data.h"
#include "util/log.h"

using namespace catter;

class ServiceImpl : public ipc::InjectService {
public:
    explicit ServiceImpl(data::ipcid_t id) : id(id) {}

    ~ServiceImpl() override = default;

    kota::task<data::ipcid_t> create(data::ipcid_t parent_id) override {
        std::println("event=create service={} parent={}", this->id, parent_id);
        co_return this->id;
    }

    kota::task<data::action> make_decision(data::command cmd) override {
        std::println(R"(event=decision executable="{}" cwd="{}" argc={})",
                     log::escape(cmd.executable),
                     log::escape(cmd.cwd),
                     cmd.args.size());
        for(size_t index = 0; index < cmd.args.size(); ++index) {
            std::println(R"(event=argument index={} value="{}")",
                         index,
                         log::escape(cmd.args[index]));
        }
        co_return data::action{.type = data::action::WRAP, .cmd = std::move(cmd)};
    }

    kota::task<> finish(data::process_result result) override {
        std::println(R"(event=finish code={} stdout="{}" stderr="{}")",
                     result.code,
                     log::escape(result.std_out),
                     log::escape(result.std_err));
        co_return;
    }

    kota::task<> report_error(data::ipcid_t parent_id, std::string error_msg) override {
        std::println(R"(event=error parent={} message="{}")", parent_id, log::escape(error_msg));
        co_return;
    }

    struct Factory {
        std::unique_ptr<ServiceImpl> operator() (data::ipcid_t id) const {
            return std::make_unique<ServiceImpl>(id);
        }
    };

private:
    data::ipcid_t id;
};

namespace {

int run_proxy(int argc, char* argv[]) {
    const std::string proxy_path = argv[1];
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc - 1));
    args.push_back(proxy_path);
    for(int index = 2; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    Session session;
    Session::ProcessLaunchPlan launch_plan{
        .executable = proxy_path,
        .args = std::move(args),
        .mode = Session::StdioMode::capture,
    };
    auto task = session.run(Session::make_run_plan(std::move(launch_plan), ServiceImpl::Factory{}));
    kota::event_loop loop;
    loop.schedule(task);
    loop.run();

    auto result = task.result();
    std::println(R"(proxy=exit code={} stdout="{}" stderr="{}")",
                 result.code,
                 log::escape(result.std_out),
                 log::escape(result.std_err));
    return static_cast<int>(result.code);
}

}  // namespace

int main(int argc, char* argv[]) {
    if(argc == 2 && std::string_view(argv[1]) == "--child") {
        std::print("child output");
        return 0;
    }

    log::mute_logger();

    try {
        return run_proxy(argc, argv);
    } catch(const std::exception& ex) {
        std::println(R"(harness=error message="{}")", log::escape(ex.what()));
        return 1;
    } catch(...) {
        std::println(R"(harness=error message="unknown exception")");
        return 1;
    }
}
