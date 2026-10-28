module;

#include "modules/prelude.h"

#include "support/messages.h"

#ifdef CATTER_MAC
#include <mach/mach_time.h>
#endif

module catter;

import :capture.eslogger;
import :capture.processes;

using namespace catter;

#ifdef CATTER_MAC

namespace {

struct Token {
    int pid;
    int pidversion;
};

std::uint64_t seq = 0;

/// The time between two lines, in Mach's absolute time.
constexpr std::uint64_t TICKS = 3'000'000;

/// The time between two lines, in microseconds.
std::int64_t microseconds_between_lines() {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return static_cast<std::int64_t>(TICKS * info.numer / info.denom / 1000);
}

/// A line as eslogger writes it, with fields catter does not read beside those
/// it does; each line comes TICKS after the one before.
std::string line(Token process, int ppid, std::string_view event) {
    seq += 1;
    return std::format(
        R"({{"schema_version":1,"mach_time":{},"global_seq_num":{},"seq_num":7,"time":"2026-10-27T10:16:14.643246583Z","action_type":1,"process":{{"audit_token":{{"asid":100058,"pid":{},"pidversion":{},"euid":501}},"ppid":{},"original_ppid":{},"is_platform_binary":true,"executable":{{"path":"/bin/bash","path_truncated":false}}}},"event":{{{}}},"version":9}})",
        seq * TICKS,
        seq,
        process.pid,
        process.pidversion,
        ppid,
        ppid,
        event);
}

std::string forked(Token parent, int ppid, Token child) {
    return line(
        parent,
        ppid,
        std::format(
            R"("fork":{{"child":{{"audit_token":{{"pid":{},"pidversion":{}}},"ppid":{}}}}})",
            child.pid,
            child.pidversion,
            parent.pid));
}

std::string execed(Token process,
                   int ppid,
                   int pidversion,
                   std::string_view path,
                   std::string_view args,
                   std::string_view cwd) {
    return line(
        process,
        ppid,
        std::format(
            R"("exec":{{"target":{{"audit_token":{{"pid":{},"pidversion":{}}},"ppid":{}}},"args":{},"cwd":{{"path":"{}","path_truncated":false}},"env":["HOME=/Users/me","PATH=/usr/bin:/bin"],"dyld_exec_path":"{}","script":null,"fds":[],"last_fd":2,"image_cputype":16777228}})",
            process.pid,
            pidversion,
            ppid,
            args,
            cwd,
            path));
}

std::string exited(Token process, int ppid, int stat) {
    return line(process, ppid, std::format(R"("exit":{{"stat":{}}})", stat));
}

bool refused(std::string_view text) {
    try {
        eslogger::parse(text);
    } catch(const cpptrace::exception&) {
        return true;
    }
    return false;
}

/// What the lines tell of the build process `build` that process 100 starts.
std::vector<std::string> reports(int build, std::initializer_list<std::string> lines) {
    data::ipcid_t last_id = 0;
    BuildProcesses processes([&] { return last_id += 1; });
    std::vector<std::string> told;
    for(const auto& each: lines) {
        for(auto& report: eslogger::follow(processes, eslogger::parse(each), 100, build)) {
            if(auto* started = std::get_if<BuildProcesses::Started>(&report)) {
                told.push_back(std::format(
                    "{} < {}: {} {} in {}",
                    started->id,
                    started->parent.has_value() ? std::to_string(*started->parent) : "-",
                    started->command.executable,
                    started->command.args,
                    started->command.cwd));
            } else {
                const auto& ended = std::get<BuildProcesses::Ended>(report);
                told.push_back(std::format("{} ended {} after {} lines",
                                           ended.id,
                                           ended.result.code,
                                           ended.result.duration / microseconds_between_lines()));
            }
        }
    }
    return told;
}

}  // namespace

ZEST_SUITE(eslogger_tests) {

ZEST_CASE(reads_what_eslogger_writes) {
    auto event = eslogger::parse(
        execed({.pid = 7, .pidversion = 1}, 6, 2, "/usr/bin/cc", R"(["cc","-c","a.c"])", "/src"));
    ZEXPECT(event.process.audit_token.pid == 7);
    ZEXPECT(event.process.ppid == 6);
    ZEXPECT(event.event.exec.has_value());
    ZEXPECT(!event.event.fork.has_value());
    ZEXPECT(event.event.exec->target.audit_token.pidversion == 2);
    ZEXPECT(event.event.exec->env ==
            std::vector<std::string>{"HOME=/Users/me", "PATH=/usr/bin:/bin"});
}

ZEST_CASE(refuses_another_schema) {
    auto text = exited({.pid = 7, .pidversion = 1}, 6, 0);
    text.replace(text.find(R"("schema_version":1)"), 18, R"("schema_version":2)");
    ZEXPECT(refused(text));
    ZEXPECT(refused(R"({"schema_version":1})"));
    auto other = exited({.pid = 7, .pidversion = 1}, 6, 0);
    other.replace(other.find(R"("exit":)"), 7, R"("open":)");
    ZEXPECT(refused(other));
}

ZEST_CASE(follows_the_build_through_forks_and_execs) {
    // The build command's process forks off this one, runs sh, which runs
    // bash, which forks a process for a compiler, which runs clang.
    const Token start{.pid = 200, .pidversion = 1};
    const Token sh{.pid = 200, .pidversion = 2};
    const Token bash{.pid = 200, .pidversion = 3};
    const Token child{.pid = 201, .pidversion = 4};
    const Token cc{.pid = 201, .pidversion = 5};
    auto told =
        reports(200,
                {
                    forked({.pid = 100, .pidversion = 9}, 1, start),
                    execed(start, 100, 2, "/bin/sh", R"(["/bin/sh","-c","cc -c a.c"])", "/src"),
                    execed(sh, 100, 3, "/bin/bash", R"(["/bin/sh","-c","cc -c a.c"])", "/src"),
                    forked(bash, 100, child),
                    execed(child, 200, 5, "/usr/bin/cc", R"(["cc","-c","a.c"])", "/src"),
                    execed(cc, 200, 6, "/usr/bin/clang", R"(["clang","-c","a.c"])", "/src"),
                    exited({.pid = 201, .pidversion = 6}, 200, 0),
                    exited(bash, 100, 3 << 8),
                });
    ZEXPECT(told == std::vector<std::string>{
                        R"(1 < -: /bin/sh ["/bin/sh", "-c", "cc -c a.c"] in /src)",
                        R"(2 < 1: /bin/bash ["/bin/sh", "-c", "cc -c a.c"] in /src)",
                        R"(3 < 2: /usr/bin/cc ["cc", "-c", "a.c"] in /src)",
                        R"(4 < 3: /usr/bin/clang ["clang", "-c", "a.c"] in /src)",
                        "4 ended 0 after 1 lines",
                        "3 ended 0 after 2 lines",
                        "2 ended 3 after 5 lines",
                        "1 ended 3 after 6 lines",
                    });
}

ZEST_CASE(leaves_out_other_processes) {
    // Process 100 starts another program beside the build; a process outside
    // the build takes the pid of one of the build's, whose exit is lost.
    const Token start{.pid = 200, .pidversion = 1};
    auto told = reports(
        200,
        {
            forked({.pid = 100, .pidversion = 9}, 1, {.pid = 300, .pidversion = 1}),
            execed({.pid = 300, .pidversion = 1}, 100, 2, "/usr/bin/true", R"(["true"])", "/"),
            forked({.pid = 100, .pidversion = 9}, 1, start),
            execed(start, 100, 2, "/usr/bin/make", R"(["make"])", "/src"),
            forked({.pid = 200, .pidversion = 2}, 100, {.pid = 201, .pidversion = 3}),
            forked({.pid = 50, .pidversion = 1}, 1, {.pid = 201, .pidversion = 7}),
            execed({.pid = 201, .pidversion = 7}, 50, 8, "/usr/bin/true", R"(["true"])", "/"),
            exited({.pid = 200, .pidversion = 2}, 100, 0),
        });
    ZEXPECT(told == std::vector<std::string>{
                        R"(1 < -: /usr/bin/make ["make"] in /src)",
                        "1 ended 0 after 4 lines",
                    });
}

ZEST_CASE(a_signal_ends_a_command_as_a_shell_tells_it) {
    const Token start{.pid = 200, .pidversion = 1};
    auto told = reports(200,
                        {
                            forked({.pid = 100, .pidversion = 9}, 1, start),
                            execed(start, 100, 2, "/usr/bin/make", R"(["make"])", "/src"),
                            exited({.pid = 200, .pidversion = 2}, 100, 9),
                        });
    ZEXPECT(told == std::vector<std::string>{
                        R"(1 < -: /usr/bin/make ["make"] in /src)",
                        "1 ended 137 after 1 lines",
                    });
}

};  // ZEST_SUITE(eslogger_tests)

#endif
