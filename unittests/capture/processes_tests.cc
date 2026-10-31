module;

#include "support/prelude.h"

#include "support/messages.h"

module catter;

import :capture.processes;

using namespace catter;

namespace {

/// A build whose processes keep their keys as they exec, as pids do, and
/// whose events come a second apart, from the session's start at 100 s: what
/// each tells, as a line.
struct Recorder {
    data::ipcid_t last_id = 0;

    BuildProcesses processes{[this] { return last_id += 1; }, chrono::seconds(100)};

    chrono::seconds now{100};

    std::vector<std::string> told;

    void exec(std::uint64_t pid, std::vector<std::string> args) {
        now += chrono::seconds(1);
        auto started = processes.exec(pid, pid, {.cwd = "/src", .args = std::move(args)}, now);
        if(started) {
            told.push_back(std::format("{} < {}: {}",
                                       started->id,
                                       started->parent ? std::to_string(*started->parent) : "-",
                                       started->command.args));
        }
    }

    void exit(std::uint64_t pid, async::process::exit_status status) {
        now += chrono::seconds(1);
        for(const auto& ended: processes.exit(pid, status, now)) {
            told.push_back(std::format("{} started at {}s, ended {} after {}s",
                                       ended.id,
                                       ended.result.start / 1'000'000,
                                       ended.result.code,
                                       ended.result.duration / 1'000'000));
        }
    }
};

}  // namespace

ZEST_SUITE(processes_tests) {

ZEST_CASE(a_command_is_a_child_of_the_one_its_process_ran) {
    // make runs sh -c, which execs the compiler in its place; make forks a
    // process for each.
    Recorder build;
    build.processes.start(10);
    build.exec(10, {"make"});
    build.processes.fork(10, 11);
    build.exec(11, {"sh", "-c", "exec cc -c a.c"});
    build.exec(11, {"cc", "-c", "a.c"});
    build.processes.fork(11, 12);
    build.exec(12, {"cc1", "a.c"});
    build.exit(12, {});
    build.exit(11, {.code = 1});
    build.exit(10, {.code = 2});
    ZEXPECT(build.told == std::vector<std::string>{
                              R"(1 < -: ["make"])",
                              R"(2 < 1: ["sh", "-c", "exec cc -c a.c"])",
                              R"(3 < 2: ["cc", "-c", "a.c"])",
                              R"(4 < 3: ["cc1", "a.c"])",
                              "4 started at 4s, ended 0 after 1s",
                              "3 started at 3s, ended 1 after 3s",
                              "2 started at 2s, ended 1 after 4s",
                              "1 started at 1s, ended 2 after 6s",
                          });
}

ZEST_CASE(a_forked_process_is_a_child_of_the_command_its_parent_ran_then) {
    // The build command's process forks before it execs make: the fork's
    // command has the build command's parent, none.
    Recorder build;
    build.processes.start(10);
    build.processes.fork(10, 11);
    build.exec(10, {"make"});
    build.exec(11, {"true"});
    ZEXPECT(build.told == std::vector<std::string>{
                              R"(1 < -: ["make"])",
                              R"(2 < -: ["true"])",
                          });
}

ZEST_CASE(leaves_out_other_processes) {
    Recorder build;
    build.processes.start(10);
    build.processes.fork(20, 21);
    build.exec(21, {"true"});
    build.exit(21, {});
    build.exec(10, {"make"});
    build.exit(10, {.term_signal = 9});
    ZEXPECT(build.told == std::vector<std::string>{
                              R"(1 < -: ["make"])",
                              "1 started at 3s, ended 137 after 1s",
                          });
}

};  // ZEST_SUITE(processes_tests)
